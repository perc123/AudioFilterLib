#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <stdexcept>
#include <memory>

/**
 * @file audio_buffer.h
 * @brief Lock-free circular (ring) buffer for real-time audio streaming
 *
 * **Overview:**
 * AudioBuffer implements a thread-safe circular buffer for producer-consumer
 * audio streaming without locks or dynamic allocation in the real-time path.
 *
 * **Key Design Principles:**
 *
 * 1. **Lock-Free (or Nearly Lock-Free):**
 *    - Uses atomic operations for index synchronization
 *    - Producer and consumer can run on different threads
 *    - No mutexes in hot path (read/write operations)
 *    - Safe for real-time threads (POSIX RT priority)
 *
 * 2. **Power-of-2 Size:**
 *    - Buffer size is always a power of 2 (e.g., 512, 1024, 2048)
 *    - Enables wraparound with bitwise AND: index & (size - 1)
 *    - Faster than modulo (%) operation
 *    - Typical: 512-4096 samples per channel
 *
 * 3. **Pre-Allocated:**
 *    - All memory allocated in constructor
 *    - No malloc/free during read/write (safe for real-time)
 *    - Latency: 1 buffer period = size / sample_rate (e.g., ~10ms @ 48kHz)
 *
 * 4. **Interleaved Multi-Channel:**
 *    - Supports stereo, surround, etc.
 *    - Layout: [L0, R0, L1, R1, ...] for stereo
 *    - Simpler than planar (less cache misses)
 *
 * **Thread Safety:**
 *
 * Producer thread (audio input):
 *   ```cpp
 *   buffer.write(left_sample, right_sample);  // One per sample time
 *   ```
 *
 * Consumer thread (DSP/filtering):
 *   ```cpp
 *   auto frame = buffer.read(256);  // Read 256 stereo samples
 *   ```
 *
 * **Ring Buffer Mechanics:**
 *
 *   Write Index (producer updates):
 *   [Produced] [Unread] [Free Space]
 *        ↑
 *     write_pos
 *
 *   Read Index (consumer updates):
 *   [Consumed] [Unread] [Free Space]
 *        ↑
 *      read_pos
 *
 * Invariant: (write_pos - read_pos) mod capacity = number of unread samples
 *
 * **Latency Considerations:**
 *
 * Audio I/O typically operates in blocks (buffers):
 *   - ALSA: 1-4 periods, 256-4096 samples each
 *   - PortAudio: callback every 256-4096 samples
 *   - Total latency ≈ 2-3 buffer periods (input + DSP + output)
 *
 * Example: 48 kHz, 1024-sample buffer:
 *   - One period: 1024/48000 ≈ 21 ms
 *   - Round-trip (in + DSP + out): ~63 ms
 *
 * **Overflow/Underflow Handling:**
 *
 * Overflow (producer catches up to consumer):
 *   - write_pos would exceed read_pos (circular)
 *   - Detection: available_write() == 0
 *   - Action: Block producer or drop oldest samples
 *
 * Underflow (consumer catches up to producer):
 *   - read_pos would exceed write_pos
 *   - Detection: available_read() == 0
 *   - Action: Block consumer or return zeros (silence)
 *
 * **Performance:**
 *
 * Typical: 20-50 CPU cycles per write() or read() call
 * - Atomic operations: ~5-10 cycles
 * - Memory copy: N*(channels) - dominated by memcpy
 * - No syscalls or lock contention
 *
 * **Usage Example:**
 *
 * ```cpp
 * // Create buffer: 1024 samples, stereo
 * AudioBuffer buffer(48000, 2, 1024);
 *
 * // Producer (audio input thread)
 * std::thread producer([&buffer]() {
 *     while (running) {
 *         float left = input_device.read();
 *         float right = input_device.read();
 *         buffer.write(left, right);  // Add one stereo sample
 *     }
 * });
 *
 * // Consumer (DSP processing thread)
 * std::thread consumer([&buffer]() {
 *     while (running) {
 *         auto frame = buffer.read(256);  // Read 256 stereo samples
 *         // Process frame...
 *         output_device.write(frame);
 *     }
 * });
 * ```
 */

namespace audiofilter {

/**
 * @class AudioBuffer
 * @brief Lock-free circular buffer for real-time multi-channel audio streaming
 *
 * Thread-safe producer-consumer buffer with O(1) read/write operations.
 * No allocation in critical path. Power-of-2 size for efficient wraparound.
 */
class AudioBuffer {
public:
    /**
     * @brief Constructor
     *
     * Allocates circular buffer and initializes for lock-free operation.
     * Buffer size is rounded up to next power-of-2.
     *
     * @param sample_rate Sample rate in Hz (e.g., 48000) - informational only
     * @param num_channels Number of channels (1=mono, 2=stereo, etc.)
     * @param capacity_samples Desired capacity in samples per channel
     *                         (will be rounded up to power-of-2)
     *
     * @throw std::invalid_argument if num_channels == 0 or capacity == 0
     *
     * **Example:**
     * ```cpp
     * AudioBuffer buffer(48000, 2, 1024);  // 1024 stereo samples @ 48kHz
     * // Actual capacity: 1024 (already power-of-2)
     * ```
     */
    AudioBuffer(uint32_t sample_rate, size_t num_channels, size_t capacity_samples);

    /**
     * @brief Destructor
     *
     * Deallocates circular buffer memory.
     */
    ~AudioBuffer() = default;

    // ===== Basic Configuration =====

    /**
     * @brief Get the sample rate
     *
     * @return Sample rate in Hz
     */
    uint32_t getSampleRate() const noexcept {
        return m_sample_rate;
    }

    /**
     * @brief Get number of channels
     *
     * @return Number of channels (1, 2, 6, etc.)
     */
    size_t getChannels() const noexcept {
        return m_num_channels;
    }

    /**
     * @brief Get buffer capacity in samples per channel
     *
     * @return Capacity (power-of-2)
     */
    size_t getCapacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief Get the capacity mask for wraparound (size - 1)
     *
     * Used internally: index & m_capacity_mask instead of index % m_capacity
     *
     * @return Bitwise AND mask for wraparound
     */
    size_t getCapacityMask() const noexcept {
        return m_capacity_mask;
    }

    // ===== Write Operations (Producer) =====

    /**
     * @brief Write a single stereo sample (convenience for 2 channels)
     *
     * Adds one sample to each of 2 channels.
     * For mono or > 2 channels, use write(const float*, size_t).
     *
     * @param left_sample Left channel sample
     * @param right_sample Right channel sample
     *
     * @throw std::runtime_error if buffer is at capacity (overflow)
     *
     * **Example:**
     * ```cpp
     * buffer.write(0.5f, -0.3f);  // Write one stereo sample
     * ```
     */
    void write(float left_sample, float right_sample);

    /**
     * @brief Write a frame of interleaved samples
     *
     * Adds num_frames samples, each with all channels.
     * Buffer layout: [L0, R0, L1, R1, ...] for stereo
     *
     * @param samples Interleaved sample buffer
     * @param num_frames Number of frames to write
     *
     * @throw std::invalid_argument if samples is nullptr
     * @throw std::runtime_error if buffer capacity exceeded (overflow)
     *
     * **Example - Mono:**
     * ```cpp
     * float mono_frame[512];
     * buffer.write(mono_frame, 512);  // 512 mono samples
     * ```
     *
     * **Example - Stereo:**
     * ```cpp
     * float stereo_frame[1024];  // 512 stereo samples (interleaved)
     * buffer.write(stereo_frame, 512);
     * ```
     */
    void write(const float* samples, size_t num_frames);

    /**
     * @brief Get number of samples available to write (free space)
     *
     * Safe to write up to this many frames without blocking.
     * Updates in real-time as consumer thread reads.
     *
     * @return Number of frames that can be written without overflow
     */
    size_t availableWrite() const noexcept;

    // ===== Read Operations (Consumer) =====

    /**
     * @brief Read a frame of interleaved samples
     *
     * Blocks if insufficient data available (configurable behavior).
     * Returns vector of interleaved samples.
     *
     * @param num_frames Number of frames to read
     *
     * @return Vector of interleaved samples (size = num_frames * num_channels)
     *         Returns silence (zeros) if underflow
     *
     * @throw std::invalid_argument if num_frames == 0
     *
     * **Example - Stereo:**
     * ```cpp
     * auto frame = buffer.read(256);
     * // frame.size() == 256 * 2 == 512 (256 stereo samples)
     * ```
     */
    std::vector<float> read(size_t num_frames);

    /**
     * @brief Read a frame into a pre-allocated buffer
     *
     * More efficient than vector-returning read() (avoids allocation).
     * Use this in real-time loops.
     *
     * @param output Pre-allocated buffer (size >= num_frames * num_channels)
     * @param num_frames Number of frames to read
     *
     * @throw std::invalid_argument if output is nullptr or buffer too small
     * @throw std::runtime_error if underflow (insufficient data)
     *
     * **Example:**
     * ```cpp
     * float output_buffer[1024];  // Pre-allocated
     * buffer.read(output_buffer, 512);  // Read 512 stereo samples
     * ```
     */
    void read(float* output, size_t num_frames);

    /**
     * @brief Get number of samples available to read (unread data)
     *
     * Safe to read up to this many frames without blocking.
     *
     * @return Number of frames available to read
     */
    size_t availableRead() const noexcept;

    /**
     * @brief Peek at data without removing it from buffer
     *
     * Read data but keep write pointer unchanged.
     * Useful for analysis or lookahead filtering.
     *
     * @param num_frames Number of frames to peek
     * @return Vector of peeked samples
     */
    std::vector<float> peek(size_t num_frames) const;

    // ===== Buffer Control =====

    /**
     * @brief Clear the buffer (discard all data)
     *
     * Safe to call from either thread, but not during concurrent reads/writes.
     * Typically called during initialization or stop.
     */
    void clear() noexcept;

    /**
     * @brief Check if buffer is empty
     *
     * @return true if no unread samples, false otherwise
     */
    bool isEmpty() const noexcept {
        return availableRead() == 0;
    }

    /**
     * @brief Check if buffer is full
     *
     * @return true if no free space, false otherwise
     */
    bool isFull() const noexcept {
        return availableWrite() == 0;
    }

    /**
     * @brief Get current fill level as percentage
     *
     * Useful for monitoring/debugging.
     *
     * @return Fill level in range [0, 100]
     */
    float getFillPercentage() const noexcept;

    // ===== Statistics (for debugging/monitoring) =====

    /**
     * @brief Get total number of samples written since creation
     *
     * Monotonically increasing counter.
     *
     * @return Total samples written
     */
    uint64_t getTotalWritten() const noexcept {
        return m_total_written.load(std::memory_order_acquire);
    }

    /**
     * @brief Get total number of samples read since creation
     *
     * Monotonically increasing counter.
     *
     * @return Total samples read
     */
    uint64_t getTotalRead() const noexcept {
        return m_total_read.load(std::memory_order_acquire);
    }

    /**
     * @brief Get number of overflow events (write blocked by full buffer)
     *
     * Non-zero indicates producer/consumer mismatch.
     *
     * @return Overflow event count
     */
    uint32_t getOverflowCount() const noexcept {
        return m_overflow_count.load(std::memory_order_acquire);
    }

    /**
     * @brief Get number of underflow events (read blocked by empty buffer)
     *
     * Non-zero indicates producer/consumer mismatch.
     *
     * @return Underflow event count
     */
    uint32_t getUnderflowCount() const noexcept {
        return m_underflow_count.load(std::memory_order_acquire);
    }

    /**
     * @brief Reset statistics counters
     *
     * Useful for periodic monitoring during operation.
     */
    void resetStatistics() noexcept;

private:
    // ===== Data Members =====

    uint32_t m_sample_rate;      ///< Sample rate in Hz
    size_t m_num_channels;        ///< Number of channels
    size_t m_capacity;            ///< Buffer capacity (power-of-2)
    size_t m_capacity_mask;       ///< Bitwise mask for wraparound (capacity - 1)

    std::vector<float> m_buffer;  ///< Circular buffer storage

    // Atomic indices for lock-free synchronization
    std::atomic<size_t> m_write_index{0};  ///< Producer write position
    std::atomic<size_t> m_read_index{0};   ///< Consumer read position

    // Statistics
    std::atomic<uint64_t> m_total_written{0};   ///< Total samples written
    std::atomic<uint64_t> m_total_read{0};      ///< Total samples read
    std::atomic<uint32_t> m_overflow_count{0};  ///< Write overflow events
    std::atomic<uint32_t> m_underflow_count{0}; ///< Read underflow events

    // ===== Private Helpers =====

    /**
     * @brief Round up to next power-of-2
     *
     * @param value Input value
     * @return Smallest power-of-2 >= value
     */
    static size_t roundToPowerOfTwo(size_t value);

    /**
     * @brief Check if value is power-of-2
     *
     * @param value Input value
     * @return true if value is a power-of-2, false otherwise
     */
    static bool isPowerOfTwo(size_t value) {
        return value > 0 && (value & (value - 1)) == 0;
    }
};

}  // namespace audiofilter
