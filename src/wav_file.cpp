/**
 * @file wav_file.cpp
 * @brief Implementation of WAV file I/O wrapper
 */

#include "wav_file.h"
#include <sndfile.h>
#include <stdexcept>
#include <sstream>

namespace audiofilter {

// ===== Constructor/Destructor =====

WAVFile::WAVFile(const std::string& filename) : m_read_mode(true) {
    SF_INFO sfInfo;
    std::memset(&sfInfo, 0, sizeof(sfInfo));

    // Open file for reading
    m_file_handle = sf_open(filename.c_str(), SFM_READ, &sfInfo);

    if (m_file_handle == nullptr) {
        std::ostringstream oss;
        oss << "WAVFile: Failed to open file '" << filename << "': "
            << sf_strerror(nullptr);
        throw std::runtime_error(oss.str());
    }

    // Store file info
    m_sample_rate = sfInfo.samplerate;
    m_num_channels = sfInfo.channels;
    m_num_frames = sfInfo.frames;

    if (m_sample_rate == 0 || m_num_channels == 0) {
        sf_close(m_file_handle);
        m_file_handle = nullptr;
        throw std::runtime_error("WAVFile: Invalid audio file (zero sample rate or channels)");
    }
}

WAVFile::WAVFile(const std::string& filename, uint32_t sample_rate, int num_channels)
    : m_sample_rate(sample_rate), m_num_channels(num_channels), m_read_mode(false) {

    SF_INFO sfInfo;
    std::memset(&sfInfo, 0, sizeof(sfInfo));

    sfInfo.samplerate = sample_rate;
    sfInfo.channels = num_channels;
    sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;  // 16-bit PCM WAV

    // Open file for writing
    m_file_handle = sf_open(filename.c_str(), SFM_WRITE, &sfInfo);

    if (m_file_handle == nullptr) {
        std::ostringstream oss;
        oss << "WAVFile: Failed to create file '" << filename << "': "
            << sf_strerror(nullptr);
        throw std::runtime_error(oss.str());
    }

    m_num_frames = 0;
}

WAVFile::~WAVFile() {
    close();
}

// ===== File Control =====

void WAVFile::close() noexcept {
    if (m_file_handle != nullptr) {
        sf_close(m_file_handle);
        m_file_handle = nullptr;
    }
}

void WAVFile::ensureOpen(const std::string& operation) const {
    if (m_file_handle == nullptr) {
        throw std::runtime_error("WAVFile: File not open (" + operation + ")");
    }
}

// ===== Reading =====

std::vector<float> WAVFile::readFrames(uint64_t num_frames) {
    ensureOpen("readFrames");

    if (!m_read_mode) {
        throw std::runtime_error("WAVFile: File opened for writing, cannot read");
    }

    if (num_frames == 0) {
        return std::vector<float>();
    }

    // Allocate buffer for interleaved samples
    size_t num_samples = num_frames * m_num_channels;
    std::vector<float> buffer(num_samples);

    // Read from libsndfile
    sf_count_t frames_read = sf_readf_float(m_file_handle, buffer.data(), num_frames);

    if (frames_read < 0) {
        throw std::runtime_error(std::string("WAVFile: Read error: ") + sf_strerror(m_file_handle));
    }

    // Trim buffer to actual frames read
    if (frames_read < static_cast<sf_count_t>(num_frames)) {
        buffer.resize(frames_read * m_num_channels);
    }

    return buffer;
}

std::vector<float> WAVFile::readAll() {
    ensureOpen("readAll");

    if (!m_read_mode) {
        throw std::runtime_error("WAVFile: File opened for writing, cannot read");
    }

    // Calculate remaining frames
    uint64_t current_pos = getCurrentFrame();
    uint64_t remaining = m_num_frames - current_pos;

    if (remaining == 0) {
        return std::vector<float>();
    }

    return readFrames(remaining);
}

void WAVFile::seekFrame(uint64_t frame_pos) {
    ensureOpen("seekFrame");

    sf_count_t result = sf_seek(m_file_handle, frame_pos, SEEK_SET);

    if (result < 0) {
        throw std::runtime_error(std::string("WAVFile: Seek error: ") + sf_strerror(m_file_handle));
    }
}

uint64_t WAVFile::getCurrentFrame() const noexcept {
    if (m_file_handle == nullptr) {
        return 0;
    }

    sf_count_t pos = sf_seek(m_file_handle, 0, SEEK_CUR);
    return (pos < 0) ? 0 : static_cast<uint64_t>(pos);
}

// ===== Writing =====

void WAVFile::writeFrames(const float* samples, uint64_t num_frames) {
    ensureOpen("writeFrames");

    if (!m_read_mode == false) {
        throw std::runtime_error("WAVFile: File opened for reading, cannot write");
    }

    if (samples == nullptr && num_frames > 0) {
        throw std::invalid_argument("WAVFile::writeFrames: samples pointer is nullptr");
    }

    if (num_frames == 0) {
        return;
    }

    // Write to libsndfile
    sf_count_t frames_written = sf_writef_float(m_file_handle, samples, num_frames);

    if (frames_written < 0) {
        throw std::runtime_error(std::string("WAVFile: Write error: ") + sf_strerror(m_file_handle));
    }

    if (frames_written < static_cast<sf_count_t>(num_frames)) {
        throw std::runtime_error("WAVFile: Incomplete write");
    }

    m_num_frames += frames_written;
}

}  // namespace audiofilter
