#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <memory>

/**
 * @file filter_base.h
 * @brief Abstract base class for audio filters (IIR, FIR, etc.)
 *
 * Defines the common interface for all digital filter implementations.
 * Supports both real-time sample-by-sample and frame-based processing.
 */

namespace audiofilter {

/**
 * @class FilterBase
 * @brief Abstract base class for all digital audio filters
 *
 * Provides the interface that all filter implementations (IIR, FIR, adaptive, etc.)
 * must conform to. Handles sample rate and channel configuration.
 *
 * **Usage:**
 * ```cpp
 * std::unique_ptr<FilterBase> filter = designer.createButterworthLowpass(...);
 * filter->reset();  // Clear internal state
 *
 * // Sample-by-sample processing
 * float output = filter->process(input_sample);
 *
 * // Or frame-based processing (more efficient for offline)
 * float buffer[512];
 * filter->processFrame(buffer, 512);
 * ```
 *
 * **Thread Safety:**
 * - `process()` and `processFrame()` are NOT thread-safe
 * - Multiple threads must use separate filter instances
 * - Configuration changes (cutoff, gain) may not be safe during processing
 *
 * **Real-Time Constraints:**
 * - `process()` should execute in O(N) time where N is filter order
 * - No dynamic memory allocation in hot path
 * - Suitable for sample rates up to 192 kHz
 */
class FilterBase {
public:
    /// Default sample rate (Hz)
    static constexpr uint32_t DEFAULT_SAMPLE_RATE = 48000;

    /// Maximum supported channels for multi-channel processing
    static constexpr size_t MAX_CHANNELS = 8;

    /**
     * @brief Virtual destructor for polymorphic deletion
     */
    virtual ~FilterBase() = default;

    // ===== Core Processing Methods =====

    /**
     * @brief Process a single audio sample
     *
     * Applies the filter to a single input sample and returns the filtered output.
     * Maintains internal state for difference equation evaluation.
     *
     * **Complexity:** O(N) where N is filter order (typically 2-4 for biquads)
     *
     * @param input_sample Input sample value (typically in range [-1.0, 1.0])
     * @return Filtered output sample
     *
     * @throw std::runtime_error if filter is not configured
     *
     * **Example:**
     * ```cpp
     * float output = filter->process(0.5f);
     * ```
     */
    virtual float process(float input_sample) = 0;

    /**
     * @brief Process a frame of audio samples (in-place)
     *
     * Processes multiple samples in sequence, updating buffer in-place.
     * More efficient than repeated `process()` calls due to better cache locality.
     *
     * **Complexity:** O(N * num_samples)
     *
     * @param buffer Pointer to sample buffer [IN/OUT]
     * @param num_samples Number of samples to process
     *
     * @throw std::invalid_argument if buffer is nullptr or num_samples == 0
     * @throw std::runtime_error if filter is not configured
     *
     * **Interleaved Multi-Channel Example:**
     * ```cpp
     * // Stereo buffer: [L0, R0, L1, R1, L2, R2, ...]
     * float buffer[256];  // 128 stereo samples
     * filter->processFrame(buffer, 256);
     * ```
     */
    virtual void processFrame(float* buffer, size_t num_samples) = 0;

    /**
     * @brief Process interleaved multi-channel audio frame
     *
     * Processes N-channel interleaved audio in a single call.
     * Each filter instance processes one channel (separate state).
     * Caller is responsible for dispatching channels to different filter instances.
     *
     * **Buffer Layout:**
     * For stereo (2 channels), `buffer[0..2N-1]` arranged as:
     * `[L0, R0, L1, R1, L2, R2, ..., L_{N-1}, R_{N-1}]`
     *
     * @param buffer Interleaved sample buffer [IN/OUT]
     * @param num_frames Number of frames (sample groups, not total samples)
     * @param num_channels Number of channels per frame
     *
     * @throw std::invalid_argument if num_channels > MAX_CHANNELS or == 0
     * @throw std::runtime_error if filter is not configured
     *
     * **Example - Processing Stereo:**
     * ```cpp
     * // Option A: Use single filter per channel
     * auto left_filter = designer.createLowpass(...);
     * auto right_filter = designer.createLowpass(...);
     *
     * float stereo_buffer[512];  // 256 stereo samples
     * // Separate channels manually and process
     *
     * // Option B: Use processInterleaved if filter supports it
     * // (caller's responsibility to match channels to filter state)
     * ```
     */
    virtual void processInterleaved(float* buffer, size_t num_frames,
                                   size_t num_channels) = 0;

    // ===== Configuration Methods =====

    /**
     * @brief Reset filter state to initial condition
     *
     * Clears all internal state (delay lines, history buffers) without
     * changing filter coefficients. Safe to call between audio blocks.
     *
     * **Behavior:**
     * - For IIR: Clear internal state w[n] (delay line)
     * - For FIR: Clear input history buffer
     * - Preserves filter coefficients (cutoff, Q, gain)
     *
     * **Use Cases:**
     * - Between independent audio segments
     * - When resuming playback after pause
     * - To eliminate discontinuities from previous filtering
     *
     * **Example:**
     * ```cpp
     * filter->reset();  // Start fresh filtering
     * ```
     */
    virtual void reset() noexcept = 0;

    /**
     * @brief Configure filter for the given sample rate and channels
     *
     * Must be called before any `process()` calls.
     * Different sample rates may require recomputation of coefficients.
     *
     * @param sample_rate Sample rate in Hz (e.g., 44100, 48000, 96000)
     * @param num_channels Number of channels to process (1=mono, 2=stereo, etc.)
     *
     * @throw std::invalid_argument if sample_rate < 8000 or > 192000
     * @throw std::invalid_argument if num_channels == 0 or > MAX_CHANNELS
     *
     * **Example:**
     * ```cpp
     * filter->configure(48000, 2);  // 48 kHz stereo
     * ```
     */
    virtual void configure(uint32_t sample_rate, size_t num_channels = 1) = 0;

    // ===== State Queries =====

    /**
     * @brief Get the configured sample rate
     *
     * @return Sample rate in Hz, or 0 if not configured
     */
    virtual uint32_t getSampleRate() const noexcept = 0;

    /**
     * @brief Get the number of configured channels
     *
     * @return Number of channels, or 0 if not configured
     */
    virtual size_t getChannels() const noexcept = 0;

    /**
     * @brief Check if filter is configured and ready to process
     *
     * @return true if sample rate and channels are set, false otherwise
     */
    virtual bool isConfigured() const noexcept = 0;

    /**
     * @brief Get filter name/description
     *
     * @return Human-readable filter name (e.g., "Butterworth Lowpass 4th Order")
     */
    virtual const char* getName() const noexcept = 0;

    // ===== Advanced Queries =====

    /**
     * @brief Get filter order (for IIR: 2*num_biquads, for FIR: num_taps)
     *
     * @return Filter order, or 0 if not applicable
     */
    virtual size_t getOrder() const noexcept = 0;

    /**
     * @brief Get estimated group delay in samples at the cutoff frequency
     *
     * For IIR: phase delay at cutoff
     * For FIR: (num_taps - 1) / 2 (symmetric design)
     *
     * @return Group delay in samples, or -1 if not applicable
     */
    virtual float getGroupDelay() const noexcept = 0;

    /**
     * @brief Estimate the number of CPU cycles per sample at current settings
     *
     * Used for performance budgeting in real-time systems.
     * Only accurate after benchmarking on target CPU.
     *
     * @return Approximate CPU cycles per sample, or 0 if unknown
     */
    virtual uint32_t getCyclesPerSample() const noexcept = 0;

protected:
    /// Sample rate in Hz
    uint32_t m_sample_rate = DEFAULT_SAMPLE_RATE;

    /// Number of channels
    size_t m_num_channels = 1;

    /// Indicates if filter has been configured
    bool m_configured = false;

    /**
     * @brief Protected constructor
     *
     * Only subclasses can instantiate FilterBase (it's abstract).
     */
    FilterBase() = default;

    /**
     * @brief Helper to validate sample rate
     *
     * @throw std::invalid_argument if sample_rate is out of valid range
     */
    static void validateSampleRate(uint32_t sample_rate) {
        if (sample_rate < 8000 || sample_rate > 192000) {
            throw std::invalid_argument(
                "Sample rate must be between 8000 and 192000 Hz"
            );
        }
    }

    /**
     * @brief Helper to validate channel count
     *
     * @throw std::invalid_argument if num_channels is invalid
     */
    static void validateChannels(size_t num_channels) {
        if (num_channels == 0 || num_channels > MAX_CHANNELS) {
            throw std::invalid_argument(
                "Number of channels must be 1-" +
                std::to_string(MAX_CHANNELS)
            );
        }
    }
};

/**
 * @brief Unique pointer to a filter (common pattern)
 */
using FilterPtr = std::unique_ptr<FilterBase>;

}  // namespace audiofilter
