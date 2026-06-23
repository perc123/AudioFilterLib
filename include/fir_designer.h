#pragma once

#include "biquad_filter.h"
#include <vector>
#include <memory>
#include <cmath>
#include <stdexcept>

/**
 * @file fir_designer.h
 * @brief FIR filter design using windowed sinc method
 *
 * **Overview:**
 * FIR (Finite Impulse Response) filters have impulse responses of finite length.
 * Unlike IIR filters (recursive), FIR filters are non-recursive and guaranteed stable.
 *
 * **Key Properties:**
 * - **Stability:** Always stable (no feedback, finite duration)
 * - **Phase:** Can achieve exactly linear phase (symmetric taps)
 * - **Order:** Requires higher order than IIR for same frequency response
 * - **Latency:** Group delay = (N-1)/2 samples (known and constant)
 *
 * **FIR vs IIR:**
 *
 * | Property | FIR | IIR |
 * |----------|-----|-----|
 * | Stability | Always | Conditional (poles in unit circle) |
 * | Phase | Linear (exact) | Non-linear |
 * | Order | High (100+) | Low (2-8) |
 * | CPU | More ops/sample | Fewer ops/sample |
 * | Group Delay | (N-1)/2 | Frequency-dependent |
 * | Applications | Zero-phase, offline | Real-time, tight budget |
 *
 * **Windowed Sinc Method:**
 *
 * Step 1: Ideal Sinc Impulse Response
 *   h_ideal[n] = sin(2π*fc*n) / (π*n)  for n ≠ 0
 *   h_ideal[0] = 2*fc  (limit as n→0)
 * where fc = normalized cutoff frequency (0 < fc < 1)
 *
 * Step 2: Apply Window
 *   h_windowed[n] = h_ideal[n] * w[n]
 * where w[n] is a window function (Hamming, Blackman, Kaiser)
 *
 * Step 3: Normalize
 *   h_final = h_windowed / sum(h_windowed)  for unity DC gain
 *
 * **Window Functions:**
 *
 * Hamming:
 *   w[n] = 0.54 - 0.46*cos(2π*n/(N-1))
 *   Simple, main lobe ≈ 1.3π/N, side lobes ≈ -43 dB
 *
 * Blackman:
 *   w[n] = 0.42 - 0.5*cos(...) + 0.08*cos(...)
 *   Better side lobes (-58 dB), wider main lobe
 *
 * Kaiser:
 *   w[n] = I₀(β*√(1-(2n/(N-1)-1)²)) / I₀(β)
 *   Parametric: β controls side lobe attenuation
 *   β ≈ 0.1102*(ripple_dB - 8.7) for desired ripple
 *
 * **Frequency Response:**
 *   The sinc function |sin(x)/x| in frequency domain creates:
 *   - Main lobe centered at cutoff (width ≈ 4π/N)
 *   - Side lobes at ±π, ±2π, ... (magnitude depends on window)
 *
 * **Filter Length vs. Transition Width:**
 *   Narrower transition (sharper rolloff) → longer filter
 *   N ≈ 4/ΔF where ΔF = transition width (normalized, 0-1 scale)
 *
 * **Usage:**
 *
 * ```cpp
 * FIRDesigner designer;
 *
 * // Design lowpass with -80 dB stopband attenuation
 * auto taps = designer.designKaiserLowpass(
 *     48000,        // sample rate
 *     5000,         // cutoff frequency
 *     80,           // stopband attenuation (dB)
 *     200           // transition width (Hz)
 * );
 *
 * // Create FIR filter object
 * auto fir = designer.createFIRFilter(taps);
 * fir->configure(48000, 1);
 * fir->reset();
 *
 * // Filter audio
 * for (size_t i = 0; i < num_samples; ++i) {
 *     output[i] = fir->process(input[i]);
 * }
 * ```
 *
 * **Performance Considerations:**
 *
 * Direct Convolution:
 *   y[n] = Σ h[k]*x[n-k] for k=0..N-1
 *   Cost: O(N) per sample, N multiplications
 *   Suitable for: N < 64, real-time with low latency
 *
 * FFT-based Convolution:
 *   Use FFT for overlap-add or overlap-save
 *   Cost: O(N*log(N)) amortized per block
 *   Suitable for: N > 256, offline processing, lower CPU per sample
 *
 * This implementation uses direct convolution. For large N, consider FFT.
 */

namespace audiofilter {

/**
 * @enum WindowType
 * @brief Window functions for FIR design
 */
enum class WindowType {
    Hamming,   ///< Simple window (-43 dB side lobes)
    Blackman,  ///< Better attenuation (-58 dB side lobes)
    Kaiser     ///< Parametric (adjustable via beta parameter)
};

/**
 * @class FIRFilter
 * @brief FIR filter implementation using direct convolution
 *
 * Stores filter taps and maintains history buffer for non-recursive filtering.
 * Guarantees linear phase for symmetric tap design.
 */
class FIRFilter : public FilterBase {
public:
    /**
     * @brief Constructor with tap coefficients
     *
     * @param taps FIR filter tap coefficients (impulse response)
     *
     * @throw std::invalid_argument if taps is empty
     */
    explicit FIRFilter(const std::vector<float>& taps);

    // ===== Virtual Method Implementations =====

    float process(float input_sample) override;

    void processFrame(float* buffer, size_t num_samples) override;

    void processInterleaved(float* buffer, size_t num_frames,
                           size_t num_channels) override;

    void reset() noexcept override;

    void configure(uint32_t sample_rate, size_t num_channels = 1) override;

    uint32_t getSampleRate() const noexcept override {
        return m_sample_rate;
    }

    size_t getChannels() const noexcept override {
        return m_num_channels;
    }

    bool isConfigured() const noexcept override {
        return m_sample_rate > 0;
    }

    const char* getName() const noexcept override {
        return "FIRFilter (Direct Convolution)";
    }

    size_t getOrder() const noexcept override {
        return m_taps.size();
    }

    float getGroupDelay() const noexcept override {
        return static_cast<float>(m_taps.size() - 1) / 2.0f;
    }

    uint32_t getCyclesPerSample() const noexcept override {
        return static_cast<uint32_t>(m_taps.size());
    }

    // ===== FIR-Specific Methods =====

    /**
     * @brief Get the filter tap coefficients
     *
     * @return Const reference to tap vector
     */
    const std::vector<float>& getTaps() const noexcept {
        return m_taps;
    }

    /**
     * @brief Get the size (number of taps)
     *
     * @return Number of coefficients
     */
    size_t size() const noexcept {
        return m_taps.size();
    }

private:
    std::vector<float> m_taps;           ///< Filter coefficients
    std::vector<float> m_history;        ///< Input history buffer (circular)
    size_t m_history_index = 0;          ///< Current write position in circular buffer
};

/**
 * @class FIRDesigner
 * @brief Designs FIR filters using windowed sinc method
 */
class FIRDesigner {
public:
    // ===== Kaiser Window Designs =====

    /**
     * @brief Design lowpass FIR using Kaiser window
     *
     * Automatically computes filter length and Kaiser β parameter
     * based on desired stopband attenuation.
     *
     * **Parameters:**
     * - sample_rate: Sampling frequency (Hz)
     * - cutoff_freq: Cutoff frequency (-3dB point) in Hz
     * - stopband_atten_db: Minimum stopband attenuation in dB (e.g., 80)
     * - transition_width_hz: Width of transition band (Hz)
     *   Narrower → longer filter, sharper rolloff
     *   Typical: 100-500 Hz depending on cutoff frequency
     *
     * **Filter Length Estimation:**
     *   N ≈ stopband_atten_dB / (22.0 * transition_width_normalized)
     *   where transition_width_normalized = transition_width_hz / sample_rate
     *
     * @param sample_rate Sample rate in Hz
     * @param cutoff_freq Cutoff frequency in Hz
     * @param stopband_atten_db Stopband attenuation in dB (e.g., 60-120)
     * @param transition_width_hz Transition band width in Hz
     *
     * @return Vector of FIR tap coefficients
     *
     * @throw std::invalid_argument if parameters out of range
     */
    std::vector<float> designKaiserLowpass(
        uint32_t sample_rate, float cutoff_freq,
        float stopband_atten_db, float transition_width_hz);

    /**
     * @brief Design highpass FIR using Kaiser window
     *
     * @param sample_rate Sample rate in Hz
     * @param cutoff_freq Cutoff frequency in Hz
     * @param stopband_atten_db Stopband attenuation in dB
     * @param transition_width_hz Transition band width in Hz
     *
     * @return Vector of FIR tap coefficients
     */
    std::vector<float> designKaiserHighpass(
        uint32_t sample_rate, float cutoff_freq,
        float stopband_atten_db, float transition_width_hz);

    /**
     * @brief Design bandpass FIR using Kaiser window
     *
     * @param sample_rate Sample rate in Hz
     * @param center_freq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param stopband_atten_db Stopband attenuation in dB
     *
     * @return Vector of FIR tap coefficients
     */
    std::vector<float> designKaiserBandpass(
        uint32_t sample_rate, float center_freq, float bandwidth,
        float stopband_atten_db);

    /**
     * @brief Design bandstop (notch) FIR using Kaiser window
     *
     * @param sample_rate Sample rate in Hz
     * @param center_freq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param stopband_atten_db Stopband attenuation in dB
     *
     * @return Vector of FIR tap coefficients
     */
    std::vector<float> designKaiserBandstop(
        uint32_t sample_rate, float center_freq, float bandwidth,
        float stopband_atten_db);

    // ===== Hamming Window Designs =====

    /**
     * @brief Design lowpass FIR using Hamming window
     *
     * Simpler than Kaiser, but less control over frequency response.
     * Hamming window has -43 dB side lobes.
     *
     * @param sample_rate Sample rate in Hz
     * @param cutoff_freq Cutoff frequency in Hz
     * @param num_taps Number of filter taps (must be odd for symmetric design)
     *
     * @return Vector of FIR tap coefficients
     *
     * @throw std::invalid_argument if num_taps is invalid
     */
    std::vector<float> designHammingLowpass(
        uint32_t sample_rate, float cutoff_freq, size_t num_taps);

    /**
     * @brief Design highpass FIR using Hamming window
     *
     * @param sample_rate Sample rate in Hz
     * @param cutoff_freq Cutoff frequency in Hz
     * @param num_taps Number of filter taps
     *
     * @return Vector of FIR tap coefficients
     */
    std::vector<float> designHammingHighpass(
        uint32_t sample_rate, float cutoff_freq, size_t num_taps);

    /**
     * @brief Design bandpass FIR using Hamming window
     *
     * @param sample_rate Sample rate in Hz
     * @param center_freq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param num_taps Number of filter taps
     *
     * @return Vector of FIR tap coefficients
     */
    std::vector<float> designHammingBandpass(
        uint32_t sample_rate, float center_freq, float bandwidth, size_t num_taps);

    /**
     * @brief Design bandstop FIR using Hamming window
     *
     * @param sample_rate Sample rate in Hz
     * @param center_freq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param num_taps Number of filter taps
     *
     * @return Vector of FIR tap coefficients
     */
    std::vector<float> designHammingBandstop(
        uint32_t sample_rate, float center_freq, float bandwidth, size_t num_taps);

    // ===== Generic Design =====

    /**
     * @brief Design FIR with custom window function
     *
     * For advanced users who want direct control over all parameters.
     *
     * @param window_type Type of window (Hamming, Blackman, Kaiser)
     * @param normalized_cutoff Normalized cutoff (0 < fc < 1)
     * @param num_taps Number of filter taps (must be odd)
     * @param kaiser_beta Kaiser β parameter (if window_type == Kaiser)
     *
     * @return Vector of FIR tap coefficients
     */
    std::vector<float> designCustom(
        WindowType window_type, float normalized_cutoff, size_t num_taps,
        float kaiser_beta = 8.6f);

    /**
     * @brief Create a FIRFilter object from tap coefficients
     *
     * Convenience method to wrap raw taps in a FIRFilter instance.
     *
     * @param taps Filter tap coefficients
     * @return Unique pointer to configured FIRFilter
     *
     * @throw std::invalid_argument if taps is empty
     */
    std::unique_ptr<FilterBase> createFIRFilter(const std::vector<float>& taps);

private:
    // ===== Helper Functions =====

    /**
     * @brief Compute ideal sinc impulse response
     *
     * h[n] = sin(2π*fc*n) / (π*n) for n ≠ 0
     * h[0] = 2*fc (limit)
     *
     * @param normalized_cutoff Normalized cutoff frequency (0 < fc < 1)
     * @param num_taps Number of taps (must be odd)
     *
     * @return Sinc impulse response (length = num_taps)
     */
    static std::vector<float> sincImpulseResponse(
        float normalized_cutoff, size_t num_taps);

    /**
     * @brief Apply window function to impulse response
     *
     * @param impulse Input impulse response
     * @param window_type Window type
     * @param kaiser_beta Kaiser β parameter (if applicable)
     *
     * @return Windowed impulse response
     */
    static std::vector<float> applyWindow(
        const std::vector<float>& impulse, WindowType window_type, float kaiser_beta);

    /**
     * @brief Normalize taps for unity gain
     *
     * Divides all taps by their sum to ensure DC gain = 1.0
     *
     * @param[in,out] taps Filter taps to normalize
     */
    static void normalizeTaps(std::vector<float>& taps);

    /**
     * @brief Compute Hamming window values
     *
     * w[n] = 0.54 - 0.46*cos(2π*n/(N-1))
     *
     * @param num_taps Window length
     * @return Window values (size = num_taps)
     */
    static std::vector<float> hammingWindow(size_t num_taps);

    /**
     * @brief Compute Blackman window values
     *
     * w[n] = 0.42 - 0.5*cos(...) + 0.08*cos(...)
     *
     * @param num_taps Window length
     * @return Window values (size = num_taps)
     */
    static std::vector<float> blackmanWindow(size_t num_taps);

    /**
     * @brief Compute Kaiser window values
     *
     * Requires Bessel function I₀ for computation.
     *
     * @param num_taps Window length
     * @param beta Kaiser β parameter (controls side lobe level)
     * @return Window values (size = num_taps)
     */
    static std::vector<float> kaiserWindow(size_t num_taps, float beta);

    /**
     * @brief Compute modified Bessel function I₀(x)
     *
     * Used for Kaiser window computation.
     *
     * @param x Input value
     * @return I₀(x)
     */
    static double besselI0(double x);

    /**
     * @brief Estimate Kaiser β from desired stopband attenuation
     *
     * β ≈ 0.1102*(ripple_dB - 8.7)
     *
     * @param stopband_atten_db Attenuation in dB
     * @return Kaiser β parameter
     */
    static float estimateKaiserBeta(float stopband_atten_db);

    /**
     * @brief Estimate filter length for Kaiser window
     *
     * N ≈ stopband_atten / (22.0 * transition_width_normalized)
     *
     * @param stopband_atten_db Desired attenuation in dB
     * @param transition_width_norm Transition width (normalized 0-1)
     * @return Estimated number of taps
     */
    static size_t estimateFilterLength(
        float stopband_atten_db, float transition_width_norm);
};

}  // namespace audiofilter
