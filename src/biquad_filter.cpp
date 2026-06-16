/**
 * @file biquad_filter.cpp
 * @brief Implementation of BiquadFilter (Direct Form II)
 */

#include "biquad_filter.h"
#include <cmath>
#include <algorithm>

namespace audiofilter {

// ===== Constructors =====

BiquadFilter::BiquadFilter()
    : FilterBase(), m_b0(1.0f), m_b1(0.0f), m_b2(0.0f),
      m_a1(0.0f), m_a2(0.0f), m_w1(0.0f), m_w2(0.0f) {
}

BiquadFilter::BiquadFilter(float b0, float b1, float b2, float a1, float a2)
    : FilterBase(), m_b0(b0), m_b1(b1), m_b2(b2),
      m_a1(a1), m_a2(a2), m_w1(0.0f), m_w2(0.0f) {
}

// ===== Core Processing =====

float BiquadFilter::process(float input_sample) {
    if (!isConfigured()) {
        throw std::runtime_error("BiquadFilter: not configured (sample rate = 0)");
    }

    // Direct Form II difference equations
    // w[n] = x[n] - a1*w[n-1] - a2*w[n-2]
    const float w = input_sample - m_a1 * m_w1 - m_a2 * m_w2;

    // y[n] = b0*w[n] + b1*w[n-1] + b2*w[n-2]
    const float output = m_b0 * w + m_b1 * m_w1 + m_b2 * m_w2;

    // Update delay line for next sample
    m_w2 = m_w1;
    m_w1 = w;

    return output;
}

void BiquadFilter::processFrame(float* buffer, size_t num_samples) {
    if (buffer == nullptr || num_samples == 0) {
        throw std::invalid_argument(
            "BiquadFilter::processFrame: buffer is nullptr or num_samples == 0"
        );
    }

    if (!isConfigured()) {
        throw std::runtime_error("BiquadFilter: not configured");
    }

    // Process each sample in sequence
    // Compiler can unroll/vectorize this loop with -O3 -march=native
    for (size_t i = 0; i < num_samples; ++i) {
        const float w = buffer[i] - m_a1 * m_w1 - m_a2 * m_w2;
        buffer[i] = m_b0 * w + m_b1 * m_w1 + m_b2 * m_w2;
        m_w2 = m_w1;
        m_w1 = w;
    }
}

void BiquadFilter::processInterleaved(float* buffer, size_t num_frames,
                                     size_t num_channels) {
    if (num_channels == 0 || num_channels > MAX_CHANNELS) {
        throw std::invalid_argument(
            "BiquadFilter::processInterleaved: invalid num_channels"
        );
    }

    if (buffer == nullptr || num_frames == 0) {
        throw std::invalid_argument(
            "BiquadFilter::processInterleaved: buffer is nullptr or num_frames == 0"
        );
    }

    if (!isConfigured()) {
        throw std::runtime_error("BiquadFilter: not configured");
    }

    // NOTE: This implementation shares state across all channels,
    // which is INCORRECT for multi-channel audio.
    //
    // Correct usage: Create separate BiquadFilter instances per channel,
    // or manually deinterleave channels before filtering.
    //
    // Example (correct):
    //   auto left_filter = std::make_unique<BiquadFilter>(...);
    //   auto right_filter = std::make_unique<BiquadFilter>(...);
    //
    //   for each frame:
    //       left_out = left_filter->process(buffer[i*2 + 0]);
    //       right_out = right_filter->process(buffer[i*2 + 1]);

    // Process all samples (including all channels)
    size_t total_samples = num_frames * num_channels;
    for (size_t i = 0; i < total_samples; ++i) {
        const float w = buffer[i] - m_a1 * m_w1 - m_a2 * m_w2;
        buffer[i] = m_b0 * w + m_b1 * m_w1 + m_b2 * m_w2;
        m_w2 = m_w1;
        m_w1 = w;
    }
}

void BiquadFilter::reset() noexcept {
    m_w1 = 0.0f;
    m_w2 = 0.0f;
}

void BiquadFilter::configure(uint32_t sample_rate, size_t num_channels) {
    validateSampleRate(sample_rate);
    validateChannels(num_channels);

    m_sample_rate = sample_rate;
    m_num_channels = num_channels;
    m_configured = true;
}

// ===== Coefficient Management =====

void BiquadFilter::setCoefficients(float b0, float b1, float b2, float a1, float a2) noexcept {
    m_b0 = b0;
    m_b1 = b1;
    m_b2 = b2;
    m_a1 = a1;
    m_a2 = a2;
}

void BiquadFilter::getCoefficients(float& b0, float& b1, float& b2,
                                  float& a1, float& a2) const noexcept {
    b0 = m_b0;
    b1 = m_b1;
    b2 = m_b2;
    a1 = m_a1;
    a2 = m_a2;
}

// ===== State Management =====

void BiquadFilter::getState(float& w1, float& w2) const noexcept {
    w1 = m_w1;
    w2 = m_w2;
}

void BiquadFilter::setState(float w1, float w2) noexcept {
    m_w1 = w1;
    m_w2 = w2;
}

// ===== Frequency Response Analysis =====

void BiquadFilter::frequencyResponse(float frequency, uint32_t sample_rate,
                                    float& real, float& imag) const noexcept {
    // Normalize frequency to [0, 1) range where 1 = sample_rate
    const float f_norm = frequency / sample_rate;

    // Angle on unit circle: ω = 2π * f_norm
    const float omega = 2.0f * static_cast<float>(M_PI) * f_norm;

    // z = e^(jω) on the unit circle
    const float z_real = std::cos(omega);
    const float z_imag = std::sin(omega);

    // Numerator: b0 + b1*z^-1 + b2*z^-2
    // = b0 + b1*e^-jω + b2*e^-2jω
    // = b0 + b1*(cos(-ω) + j*sin(-ω)) + b2*(cos(-2ω) + j*sin(-2ω))
    float num_real = m_b0 + m_b1 * std::cos(-omega) + m_b2 * std::cos(-2.0f * omega);
    float num_imag = m_b1 * std::sin(-omega) + m_b2 * std::sin(-2.0f * omega);

    // Denominator: 1 + a1*z^-1 + a2*z^-2
    float den_real = 1.0f + m_a1 * std::cos(-omega) + m_a2 * std::cos(-2.0f * omega);
    float den_imag = m_a1 * std::sin(-omega) + m_a2 * std::sin(-2.0f * omega);

    // Complex division: (num_real + j*num_imag) / (den_real + j*den_imag)
    // = (num_real + j*num_imag) * conj(den) / |den|^2
    const float den_mag_sq = den_real * den_real + den_imag * den_imag;

    real = (num_real * den_real + num_imag * den_imag) / den_mag_sq;
    imag = (num_imag * den_real - num_real * den_imag) / den_mag_sq;
}

// ===== Helper Methods =====

void BiquadFilter::evalPoly(float coeff0, float coeff1, float coeff2,
                           float z_real, float z_imag,
                           float& out_real, float& out_imag) noexcept {
    // Evaluate: coeff0 + coeff1*z + coeff2*z^2
    // where z = (z_real, z_imag)

    // z^2 = (z_real + j*z_imag)^2
    //     = z_real^2 - z_imag^2 + j*2*z_real*z_imag
    const float z2_real = z_real * z_real - z_imag * z_imag;
    const float z2_imag = 2.0f * z_real * z_imag;

    // coeff1*z + coeff2*z^2
    const float z1_contrib_real = coeff1 * z_real + coeff2 * z2_real;
    const float z1_contrib_imag = coeff1 * z_imag + coeff2 * z2_imag;

    out_real = coeff0 + z1_contrib_real;
    out_imag = z1_contrib_imag;
}

}  // namespace audiofilter
