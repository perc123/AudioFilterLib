/**
 * @file fir_designer.cpp
 * @brief Implementation of FIR filter design using windowed sinc
 */

#include "fir_designer.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace audiofilter {

// ===== FIRFilter Implementation =====

FIRFilter::FIRFilter(const std::vector<float>& taps)
    : FilterBase(), m_taps(taps), m_history(taps.size(), 0.0f), m_history_index(0) {

    if (taps.empty()) {
        throw std::invalid_argument("FIR taps cannot be empty");
    }
}

float FIRFilter::process(float input_sample) {
    if (!isConfigured()) {
        throw std::runtime_error("FIRFilter: not configured");
    }

    // Insert input into history at current index (circular buffer)
    m_history[m_history_index] = input_sample;

    // Compute output: sum of tap * history
    float output = 0.0f;
    for (size_t k = 0; k < m_taps.size(); ++k) {
        size_t hist_idx = (m_history_index + k) % m_taps.size();
        output += m_taps[k] * m_history[hist_idx];
    }

    // Move to next position
    m_history_index = (m_history_index + m_taps.size() - 1) % m_taps.size();

    return output;
}

void FIRFilter::processFrame(float* buffer, size_t num_samples) {
    if (buffer == nullptr || num_samples == 0) {
        throw std::invalid_argument("FIRFilter::processFrame: invalid buffer");
    }

    if (!isConfigured()) {
        throw std::runtime_error("FIRFilter: not configured");
    }

    for (size_t i = 0; i < num_samples; ++i) {
        buffer[i] = process(buffer[i]);
    }
}

void FIRFilter::processInterleaved(float* buffer, size_t num_frames,
                                  size_t num_channels) {
    if (num_channels == 0 || num_channels > MAX_CHANNELS) {
        throw std::invalid_argument("FIRFilter: invalid num_channels");
    }

    if (buffer == nullptr || num_frames == 0) {
        throw std::invalid_argument("FIRFilter: invalid buffer or num_frames");
    }

    if (!isConfigured()) {
        throw std::runtime_error("FIRFilter: not configured");
    }

    size_t total_samples = num_frames * num_channels;
    for (size_t i = 0; i < total_samples; ++i) {
        buffer[i] = process(buffer[i]);
    }
}

void FIRFilter::reset() noexcept {
    std::fill(m_history.begin(), m_history.end(), 0.0f);
    m_history_index = 0;
}

void FIRFilter::configure(uint32_t sample_rate, size_t num_channels) {
    validateSampleRate(sample_rate);
    validateChannels(num_channels);

    m_sample_rate = sample_rate;
    m_num_channels = num_channels;
    m_configured = true;
}

// ===== FIRDesigner: Kaiser Window =====

std::vector<float> FIRDesigner::designKaiserLowpass(
    uint32_t sample_rate, float cutoff_freq,
    float stopband_atten_db, float transition_width_hz) {

    if (sample_rate < 8000 || sample_rate > 192000) {
        throw std::invalid_argument("Sample rate out of range");
    }
    if (cutoff_freq <= 0 || cutoff_freq >= sample_rate / 2.0f) {
        throw std::invalid_argument("Cutoff frequency out of range");
    }
    if (stopband_atten_db <= 0) {
        throw std::invalid_argument("Stopband attenuation must be > 0");
    }
    if (transition_width_hz <= 0) {
        throw std::invalid_argument("Transition width must be > 0");
    }

    // Estimate filter length
    float transition_width_norm = transition_width_hz / sample_rate;
    size_t num_taps = estimateFilterLength(stopband_atten_db, transition_width_norm);
    if (num_taps % 2 == 0) num_taps++;  // Ensure odd for symmetric design

    // Estimate Kaiser beta
    float beta = estimateKaiserBeta(stopband_atten_db);

    // Normalized cutoff frequency (0 < fc < 1)
    float fc_norm = cutoff_freq / sample_rate;

    // Generate sinc impulse response
    auto impulse = sincImpulseResponse(fc_norm, num_taps);

    // Apply Kaiser window
    auto taps = applyWindow(impulse, WindowType::Kaiser, beta);

    // Normalize to unity gain
    normalizeTaps(taps);

    return taps;
}

std::vector<float> FIRDesigner::designKaiserHighpass(
    uint32_t sample_rate, float cutoff_freq,
    float stopband_atten_db, float transition_width_hz) {

    // Design lowpass, then transform to highpass
    auto lowpass_taps = designKaiserLowpass(
        sample_rate, cutoff_freq, stopband_atten_db, transition_width_hz);

    // Highpass transformation: h_hp[n] = δ[n] - h_lp[n]
    // In practice: negate all taps and add 1 to center tap
    size_t center = lowpass_taps.size() / 2;
    for (size_t i = 0; i < lowpass_taps.size(); ++i) {
        lowpass_taps[i] = -lowpass_taps[i];
    }
    lowpass_taps[center] += 1.0f;

    // Normalize
    normalizeTaps(lowpass_taps);

    return lowpass_taps;
}

std::vector<float> FIRDesigner::designKaiserBandpass(
    uint32_t sample_rate, float center_freq, float bandwidth,
    float stopband_atten_db) {

    // Design lowpass, apply bandpass transformation
    float transition_width = 100.0f;  // Hz (reasonable default)

    auto lowpass_taps = designKaiserLowpass(
        sample_rate, bandwidth / 2.0f, stopband_atten_db, transition_width);

    // Bandpass transformation: modulate by cos(2π*fc*n)
    float wc = 2.0f * static_cast<float>(M_PI) * center_freq / sample_rate;
    size_t center = lowpass_taps.size() / 2;

    for (size_t n = 0; n < lowpass_taps.size(); ++n) {
        int offset = static_cast<int>(n) - static_cast<int>(center);
        float modulation = 2.0f * std::cos(wc * offset);
        lowpass_taps[n] *= modulation;
    }

    normalizeTaps(lowpass_taps);

    return lowpass_taps;
}

std::vector<float> FIRDesigner::designKaiserBandstop(
    uint32_t sample_rate, float center_freq, float bandwidth,
    float stopband_atten_db) {

    // Design bandpass, then transform to bandstop
    auto bandpass_taps = designKaiserBandpass(
        sample_rate, center_freq, bandwidth, stopband_atten_db);

    // Bandstop: h_bs[n] = δ[n] - h_bp[n]
    size_t center = bandpass_taps.size() / 2;
    for (size_t i = 0; i < bandpass_taps.size(); ++i) {
        bandpass_taps[i] = -bandpass_taps[i];
    }
    bandpass_taps[center] += 1.0f;

    normalizeTaps(bandpass_taps);

    return bandpass_taps;
}

// ===== FIRDesigner: Hamming Window =====

std::vector<float> FIRDesigner::designHammingLowpass(
    uint32_t sample_rate, float cutoff_freq, size_t num_taps) {

    if (num_taps == 0 || num_taps % 2 == 0) {
        throw std::invalid_argument("num_taps must be odd and > 0");
    }

    float fc_norm = cutoff_freq / sample_rate;
    auto impulse = sincImpulseResponse(fc_norm, num_taps);
    auto taps = applyWindow(impulse, WindowType::Hamming, 0.0f);
    normalizeTaps(taps);

    return taps;
}

std::vector<float> FIRDesigner::designHammingHighpass(
    uint32_t sample_rate, float cutoff_freq, size_t num_taps) {

    auto lowpass_taps = designHammingLowpass(sample_rate, cutoff_freq, num_taps);

    size_t center = num_taps / 2;
    for (size_t i = 0; i < num_taps; ++i) {
        lowpass_taps[i] = -lowpass_taps[i];
    }
    lowpass_taps[center] += 1.0f;

    normalizeTaps(lowpass_taps);

    return lowpass_taps;
}

std::vector<float> FIRDesigner::designHammingBandpass(
    uint32_t sample_rate, float center_freq, float bandwidth, size_t num_taps) {

    auto lowpass_taps = designHammingLowpass(sample_rate, bandwidth / 2.0f, num_taps);

    float wc = 2.0f * static_cast<float>(M_PI) * center_freq / sample_rate;
    int center = static_cast<int>(num_taps) / 2;

    for (size_t n = 0; n < num_taps; ++n) {
        int offset = static_cast<int>(n) - center;
        float modulation = 2.0f * std::cos(wc * offset);
        lowpass_taps[n] *= modulation;
    }

    normalizeTaps(lowpass_taps);

    return lowpass_taps;
}

std::vector<float> FIRDesigner::designHammingBandstop(
    uint32_t sample_rate, float center_freq, float bandwidth, size_t num_taps) {

    auto bandpass_taps = designHammingBandpass(sample_rate, center_freq, bandwidth, num_taps);

    int center = static_cast<int>(num_taps) / 2;
    for (size_t i = 0; i < num_taps; ++i) {
        bandpass_taps[i] = -bandpass_taps[i];
    }
    bandpass_taps[center] += 1.0f;

    normalizeTaps(bandpass_taps);

    return bandpass_taps;
}

// ===== Generic Design =====

std::vector<float> FIRDesigner::designCustom(
    WindowType window_type, float normalized_cutoff, size_t num_taps,
    float kaiser_beta) {

    if (num_taps == 0 || num_taps % 2 == 0) {
        throw std::invalid_argument("num_taps must be odd and > 0");
    }
    if (normalized_cutoff <= 0 || normalized_cutoff >= 1.0f) {
        throw std::invalid_argument("normalized_cutoff must be in (0, 1)");
    }

    auto impulse = sincImpulseResponse(normalized_cutoff, num_taps);
    auto taps = applyWindow(impulse, window_type, kaiser_beta);
    normalizeTaps(taps);

    return taps;
}

std::unique_ptr<FilterBase> FIRDesigner::createFIRFilter(
    const std::vector<float>& taps) {

    return std::make_unique<FIRFilter>(taps);
}

// ===== Helper Functions =====

std::vector<float> FIRDesigner::sincImpulseResponse(
    float normalized_cutoff, size_t num_taps) {

    std::vector<float> impulse(num_taps);
    int center = static_cast<int>(num_taps) / 2;

    for (size_t n = 0; n < num_taps; ++n) {
        int offset = static_cast<int>(n) - center;

        if (offset == 0) {
            // Limit as n→0: h[0] = 2*fc
            impulse[n] = 2.0f * normalized_cutoff;
        } else {
            // h[n] = sin(2π*fc*n) / (π*n)
            float numerator = std::sin(2.0f * static_cast<float>(M_PI) *
                                      normalized_cutoff * offset);
            float denominator = static_cast<float>(M_PI) * offset;
            impulse[n] = numerator / denominator;
        }
    }

    return impulse;
}

std::vector<float> FIRDesigner::applyWindow(
    const std::vector<float>& impulse, WindowType window_type, float kaiser_beta) {

    std::vector<float> window;

    switch (window_type) {
        case WindowType::Hamming:
            window = hammingWindow(impulse.size());
            break;
        case WindowType::Blackman:
            window = blackmanWindow(impulse.size());
            break;
        case WindowType::Kaiser:
            window = kaiserWindow(impulse.size(), kaiser_beta);
            break;
    }

    // Multiply impulse by window
    std::vector<float> windowed(impulse.size());
    for (size_t i = 0; i < impulse.size(); ++i) {
        windowed[i] = impulse[i] * window[i];
    }

    return windowed;
}

void FIRDesigner::normalizeTaps(std::vector<float>& taps) {
    float sum = std::accumulate(taps.begin(), taps.end(), 0.0f);

    if (std::abs(sum) > 1e-10f) {
        for (auto& tap : taps) {
            tap /= sum;
        }
    }
}

std::vector<float> FIRDesigner::hammingWindow(size_t num_taps) {
    std::vector<float> window(num_taps);

    for (size_t n = 0; n < num_taps; ++n) {
        float ratio = 2.0f * static_cast<float>(M_PI) * n / (num_taps - 1);
        window[n] = 0.54f - 0.46f * std::cos(ratio);
    }

    return window;
}

std::vector<float> FIRDesigner::blackmanWindow(size_t num_taps) {
    std::vector<float> window(num_taps);

    for (size_t n = 0; n < num_taps; ++n) {
        float ratio = 2.0f * static_cast<float>(M_PI) * n / (num_taps - 1);
        window[n] = 0.42f
                  - 0.5f * std::cos(ratio)
                  + 0.08f * std::cos(2.0f * ratio);
    }

    return window;
}

std::vector<float> FIRDesigner::kaiserWindow(size_t num_taps, float beta) {
    std::vector<float> window(num_taps);
    double beta_d = static_cast<double>(beta);
    double I0_beta = besselI0(beta_d);

    for (size_t n = 0; n < num_taps; ++n) {
        double x = 2.0 * n / (num_taps - 1) - 1.0;
        double arg = beta_d * std::sqrt(1.0 - x * x);
        window[n] = static_cast<float>(besselI0(arg) / I0_beta);
    }

    return window;
}

double FIRDesigner::besselI0(double x) {
    // Approximation of modified Bessel function I₀(x)
    // Using series expansion for |x| < 8, then asymptotic for larger |x|

    x = std::abs(x);

    if (x < 8.0) {
        // Series expansion
        double y = x * x;
        double sum = 1.0;
        double term = 1.0;

        for (int k = 1; k <= 50; ++k) {
            term *= y / (4.0 * k * k);
            sum += term;
            if (std::abs(term) < 1e-10) break;
        }

        return sum;
    } else {
        // Asymptotic expansion for large x
        return std::exp(x) / std::sqrt(2.0 * M_PI * x);
    }
}

float FIRDesigner::estimateKaiserBeta(float stopband_atten_db) {
    // β ≈ 0.1102*(A - 8.7) for A > 21
    if (stopband_atten_db > 21.0f) {
        return 0.1102f * (stopband_atten_db - 8.7f);
    } else {
        return 0.0f;  // No window needed for low attenuation
    }
}

size_t FIRDesigner::estimateFilterLength(
    float stopband_atten_db, float transition_width_norm) {

    // N ≈ stopband_atten / (22.0 * transition_width_normalized)
    float estimated = stopband_atten_db / (22.0f * transition_width_norm);
    return static_cast<size_t>(estimated + 0.5f);
}

}  // namespace audiofilter
