/**
 * @file audio_buffer.cpp
 * @brief Implementation of lock-free circular audio buffer
 */

#include "audio_buffer.h"
#include <algorithm>
#include <cstring>

namespace audiofilter {

// ===== Constructor =====

AudioBuffer::AudioBuffer(uint32_t sample_rate, size_t num_channels,
                         size_t capacity_samples)
    : m_sample_rate(sample_rate), m_num_channels(num_channels) {

    if (num_channels == 0) {
        throw std::invalid_argument("num_channels must be > 0");
    }
    if (capacity_samples == 0) {
        throw std::invalid_argument("capacity_samples must be > 0");
    }

    // Round capacity to next power-of-2
    m_capacity = roundToPowerOfTwo(capacity_samples);
    m_capacity_mask = m_capacity - 1;

    // Allocate circular buffer (total samples = capacity * num_channels)
    m_buffer.resize(m_capacity * m_num_channels, 0.0f);
}

// ===== Write Operations =====

void AudioBuffer::write(float left_sample, float right_sample) {
    if (m_num_channels != 2) {
        throw std::invalid_argument("write(float, float) only for stereo buffers");
    }

    float samples[2] = {left_sample, right_sample};
    write(samples, 1);
}

void AudioBuffer::write(const float* samples, size_t num_frames) {
    if (samples == nullptr) {
        throw std::invalid_argument("write: samples pointer is nullptr");
    }
    if (num_frames == 0) {
        return;  // Nothing to write
    }

    size_t samples_to_write = num_frames * m_num_channels;

    // Check for overflow
    if (availableWrite() < num_frames) {
        m_overflow_count.fetch_add(1, std::memory_order_release);
        throw std::runtime_error("AudioBuffer: write overflow (buffer full)");
    }

    // Get current write position
    size_t write_pos = m_write_index.load(std::memory_order_acquire);

    // Copy samples into circular buffer
    size_t write_end = write_pos + samples_to_write;

    if (write_end <= m_buffer.size()) {
        // No wraparound: single copy
        std::copy(samples, samples + samples_to_write,
                 m_buffer.begin() + write_pos);
    } else {
        // Wraparound: split into two copies
        size_t first_part = m_buffer.size() - write_pos;
        std::copy(samples, samples + first_part,
                 m_buffer.begin() + write_pos);
        std::copy(samples + first_part, samples + samples_to_write,
                 m_buffer.begin());
    }

    // Update write index (atomic)
    m_write_index.store((write_end) & m_capacity_mask, std::memory_order_release);

    // Update statistics
    m_total_written.fetch_add(num_frames, std::memory_order_release);
}

size_t AudioBuffer::availableWrite() const noexcept {
    return m_capacity - availableRead();
}

// ===== Read Operations =====

std::vector<float> AudioBuffer::read(size_t num_frames) {
    std::vector<float> output(num_frames * m_num_channels, 0.0f);

    try {
        read(output.data(), num_frames);
    } catch (const std::runtime_error&) {
        // Underflow: return silence
        m_underflow_count.fetch_add(1, std::memory_order_release);
    }

    return output;
}

void AudioBuffer::read(float* output, size_t num_frames) {
    if (output == nullptr) {
        throw std::invalid_argument("read: output pointer is nullptr");
    }
    if (num_frames == 0) {
        return;  // Nothing to read
    }

    size_t samples_to_read = num_frames * m_num_channels;

    // Check for underflow
    if (availableRead() < num_frames) {
        m_underflow_count.fetch_add(1, std::memory_order_release);
        throw std::runtime_error("AudioBuffer: read underflow (buffer empty)");
    }

    // Get current read position
    size_t read_pos = m_read_index.load(std::memory_order_acquire);

    // Copy samples from circular buffer
    size_t read_end = read_pos + samples_to_read;

    if (read_end <= m_buffer.size()) {
        // No wraparound: single copy
        std::copy(m_buffer.begin() + read_pos,
                 m_buffer.begin() + read_end,
                 output);
    } else {
        // Wraparound: split into two copies
        size_t first_part = m_buffer.size() - read_pos;
        std::copy(m_buffer.begin() + read_pos,
                 m_buffer.end(),
                 output);
        std::copy(m_buffer.begin(),
                 m_buffer.begin() + (read_end % m_buffer.size()),
                 output + first_part);
    }

    // Update read index (atomic)
    m_read_index.store((read_end) & m_capacity_mask, std::memory_order_release);

    // Update statistics
    m_total_read.fetch_add(num_frames, std::memory_order_release);
}

size_t AudioBuffer::availableRead() const noexcept {
    // Use monotonic totals rather than wrapped indices: when write_pos
    // wraps back around to equal read_pos, index-based math can't tell
    // an empty buffer from a full one.
    uint64_t written = m_total_written.load(std::memory_order_acquire);
    uint64_t read = m_total_read.load(std::memory_order_acquire);
    return static_cast<size_t>(written - read);
}

std::vector<float> AudioBuffer::peek(size_t num_frames) const {
    std::vector<float> output(num_frames * m_num_channels, 0.0f);

    if (num_frames == 0) {
        return output;
    }

    size_t samples_to_read = num_frames * m_num_channels;
    size_t read_pos = m_read_index.load(std::memory_order_acquire);
    size_t read_end = read_pos + samples_to_read;

    if (read_end <= m_buffer.size()) {
        std::copy(m_buffer.begin() + read_pos,
                 m_buffer.begin() + read_end,
                 output.begin());
    } else {
        size_t first_part = m_buffer.size() - read_pos;
        std::copy(m_buffer.begin() + read_pos,
                 m_buffer.end(),
                 output.begin());
        std::copy(m_buffer.begin(),
                 m_buffer.begin() + (read_end % m_buffer.size()),
                 output.begin() + first_part);
    }

    return output;
}

// ===== Buffer Control =====

void AudioBuffer::clear() noexcept {
    m_write_index.store(0, std::memory_order_release);
    m_read_index.store(0, std::memory_order_release);
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
}

float AudioBuffer::getFillPercentage() const noexcept {
    size_t available = availableRead();
    return 100.0f * available / m_capacity;
}

void AudioBuffer::resetStatistics() noexcept {
    m_overflow_count.store(0, std::memory_order_release);
    m_underflow_count.store(0, std::memory_order_release);
}

// ===== Private Helpers =====

size_t AudioBuffer::roundToPowerOfTwo(size_t value) {
    if (isPowerOfTwo(value)) {
        return value;
    }

    // Find the next power-of-2
    size_t result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

}  // namespace audiofilter
