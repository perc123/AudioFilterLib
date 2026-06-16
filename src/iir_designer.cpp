/**
 * @file iir_designer.cpp
 * @brief Implementation of IIR filter design algorithms
 */

#include "iir_designer.h"
#include <algorithm>
#include <cmath>

namespace audiofilter {

// ===== Butterworth Filters =====

std::vector<FilterPtr> IIRDesigner::designButterworthLowpass(
    uint32_t sample_rate, float cutoff_freq, size_t order) {

    if (sample_rate < 8000 || sample_rate > 192000) {
        throw std::invalid_argument("Sample rate out of range");
    }
    if (cutoff_freq <= 0 || cutoff_freq >= sample_rate / 2) {
        throw std::invalid_argument("Cutoff frequency out of range");
    }
    if (order == 0) {
        throw std::invalid_argument("Filter order must be > 0");
    }

    // Get normalized analog prototype poles (ωc = 1)
    auto s_poles = butterworthPoles(order);

    // Prewarp and scale cutoff frequency
    double wc = prewarpFrequency(cutoff_freq, sample_rate);

    // Scale poles to desired cutoff
    for (auto& pole : s_poles) {
        pole *= wc;
    }

    // Apply bilinear transform
    std::vector<std::complex<double>> z_poles;
    for (const auto& s : s_poles) {
        z_poles.push_back(bilinearTransform(s, sample_rate));
    }

    // Convert poles to biquads
    std::vector<FilterPtr> biquads;
    for (size_t i = 0; i < z_poles.size(); i += 2) {
        auto biquad = std::make_unique<BiquadFilter>();
        biquad->configure(sample_rate, 1);

        if (i + 1 < z_poles.size()) {
            // Complex conjugate pair
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i + 1], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        } else {
            // Real pole (odd order)
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        }

        biquads.push_back(std::move(biquad));
    }

    return biquads;
}

std::vector<FilterPtr> IIRDesigner::designButterworthHighpass(
    uint32_t sample_rate, float cutoff_freq, size_t order) {

    if (sample_rate < 8000 || sample_rate > 192000) {
        throw std::invalid_argument("Sample rate out of range");
    }
    if (cutoff_freq <= 0 || cutoff_freq >= sample_rate / 2) {
        throw std::invalid_argument("Cutoff frequency out of range");
    }
    if (order == 0) {
        throw std::invalid_argument("Filter order must be > 0");
    }

    // Get normalized analog prototype poles
    auto s_poles = butterworthPoles(order);

    // Prewarp cutoff
    double wc = prewarpFrequency(cutoff_freq, sample_rate);

    // Transform to highpass: s → wc/s
    transformToHighpass(s_poles, wc);

    // Apply bilinear transform
    std::vector<std::complex<double>> z_poles;
    for (const auto& s : s_poles) {
        z_poles.push_back(bilinearTransform(s, sample_rate));
    }

    // Convert poles to biquads
    std::vector<FilterPtr> biquads;
    for (size_t i = 0; i < z_poles.size(); i += 2) {
        auto biquad = std::make_unique<BiquadFilter>();
        biquad->configure(sample_rate, 1);

        if (i + 1 < z_poles.size()) {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i + 1], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        } else {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        }

        biquads.push_back(std::move(biquad));
    }

    return biquads;
}

std::vector<FilterPtr> IIRDesigner::designButterworthBandpass(
    uint32_t sample_rate, float center_freq, float bandwidth, size_t order) {

    if (sample_rate < 8000 || sample_rate > 192000) {
        throw std::invalid_argument("Sample rate out of range");
    }
    if (center_freq <= 0 || center_freq >= sample_rate / 2) {
        throw std::invalid_argument("Center frequency out of range");
    }
    if (bandwidth <= 0) {
        throw std::invalid_argument("Bandwidth must be > 0");
    }
    if (order == 0) {
        throw std::invalid_argument("Filter order must be > 0");
    }

    // Get normalized analog prototype poles
    auto s_poles = butterworthPoles(order);

    // Prewarp frequencies
    double wc0 = prewarpFrequency(center_freq, sample_rate);
    double bw_rad = freqToRadians(bandwidth);

    // Transform to bandpass
    auto z_poles_temp = transformToBandpass(s_poles, wc0, bw_rad);

    // Apply bilinear transform
    std::vector<std::complex<double>> z_poles;
    for (const auto& s : z_poles_temp) {
        z_poles.push_back(bilinearTransform(s, sample_rate));
    }

    // Convert poles to biquads
    std::vector<FilterPtr> biquads;
    for (size_t i = 0; i < z_poles.size(); i += 2) {
        auto biquad = std::make_unique<BiquadFilter>();
        biquad->configure(sample_rate, 1);

        if (i + 1 < z_poles.size()) {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i + 1], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        } else {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        }

        biquads.push_back(std::move(biquad));
    }

    return biquads;
}

std::vector<FilterPtr> IIRDesigner::designButterworthBandstop(
    uint32_t sample_rate, float center_freq, float bandwidth, size_t order) {

    if (sample_rate < 8000 || sample_rate > 192000) {
        throw std::invalid_argument("Sample rate out of range");
    }
    if (center_freq <= 0 || center_freq >= sample_rate / 2) {
        throw std::invalid_argument("Center frequency out of range");
    }
    if (bandwidth <= 0) {
        throw std::invalid_argument("Bandwidth must be > 0");
    }
    if (order == 0) {
        throw std::invalid_argument("Filter order must be > 0");
    }

    // Get normalized analog prototype poles
    auto s_poles = butterworthPoles(order);

    // Prewarp frequencies
    double wc0 = prewarpFrequency(center_freq, sample_rate);
    double bw_rad = freqToRadians(bandwidth);

    // Transform to bandstop
    auto z_poles_temp = transformToBandstop(s_poles, wc0, bw_rad);

    // Apply bilinear transform
    std::vector<std::complex<double>> z_poles;
    for (const auto& s : z_poles_temp) {
        z_poles.push_back(bilinearTransform(s, sample_rate));
    }

    // Convert poles to biquads
    std::vector<FilterPtr> biquads;
    for (size_t i = 0; i < z_poles.size(); i += 2) {
        auto biquad = std::make_unique<BiquadFilter>();
        biquad->configure(sample_rate, 1);

        if (i + 1 < z_poles.size()) {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i + 1], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        } else {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        }

        biquads.push_back(std::move(biquad));
    }

    return biquads;
}

// ===== Chebyshev Filters =====

std::vector<FilterPtr> IIRDesigner::designChebyshevLowpass(
    uint32_t sample_rate, float cutoff_freq, size_t order, float ripple_db) {

    if (ripple_db <= 0) {
        throw std::invalid_argument("Ripple must be > 0 dB");
    }

    // Get normalized analog prototype poles (ωc = 1)
    auto s_poles = chebyshevPoles(order, ripple_db);

    // Prewarp and scale cutoff frequency
    double wc = prewarpFrequency(cutoff_freq, sample_rate);

    // Scale poles to desired cutoff
    for (auto& pole : s_poles) {
        pole *= wc;
    }

    // Apply bilinear transform
    std::vector<std::complex<double>> z_poles;
    for (const auto& s : s_poles) {
        z_poles.push_back(bilinearTransform(s, sample_rate));
    }

    // Convert poles to biquads
    std::vector<FilterPtr> biquads;
    for (size_t i = 0; i < z_poles.size(); i += 2) {
        auto biquad = std::make_unique<BiquadFilter>();
        biquad->configure(sample_rate, 1);

        if (i + 1 < z_poles.size()) {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i + 1], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        } else {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        }

        biquads.push_back(std::move(biquad));
    }

    return biquads;
}

std::vector<FilterPtr> IIRDesigner::designChebyshevHighpass(
    uint32_t sample_rate, float cutoff_freq, size_t order, float ripple_db) {

    if (ripple_db <= 0) {
        throw std::invalid_argument("Ripple must be > 0 dB");
    }

    auto s_poles = chebyshevPoles(order, ripple_db);
    double wc = prewarpFrequency(cutoff_freq, sample_rate);
    transformToHighpass(s_poles, wc);

    std::vector<std::complex<double>> z_poles;
    for (const auto& s : s_poles) {
        z_poles.push_back(bilinearTransform(s, sample_rate));
    }

    std::vector<FilterPtr> biquads;
    for (size_t i = 0; i < z_poles.size(); i += 2) {
        auto biquad = std::make_unique<BiquadFilter>();
        biquad->configure(sample_rate, 1);

        if (i + 1 < z_poles.size()) {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i + 1], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        } else {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        }

        biquads.push_back(std::move(biquad));
    }

    return biquads;
}

std::vector<FilterPtr> IIRDesigner::designChebyshevBandpass(
    uint32_t sample_rate, float center_freq, float bandwidth,
    size_t order, float ripple_db) {

    if (ripple_db <= 0) {
        throw std::invalid_argument("Ripple must be > 0 dB");
    }

    auto s_poles = chebyshevPoles(order, ripple_db);
    double wc0 = prewarpFrequency(center_freq, sample_rate);
    double bw_rad = freqToRadians(bandwidth);

    auto z_poles_temp = transformToBandpass(s_poles, wc0, bw_rad);

    std::vector<std::complex<double>> z_poles;
    for (const auto& s : z_poles_temp) {
        z_poles.push_back(bilinearTransform(s, sample_rate));
    }

    std::vector<FilterPtr> biquads;
    for (size_t i = 0; i < z_poles.size(); i += 2) {
        auto biquad = std::make_unique<BiquadFilter>();
        biquad->configure(sample_rate, 1);

        if (i + 1 < z_poles.size()) {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i + 1], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        } else {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        }

        biquads.push_back(std::move(biquad));
    }

    return biquads;
}

std::vector<FilterPtr> IIRDesigner::designChebyshevBandstop(
    uint32_t sample_rate, float center_freq, float bandwidth,
    size_t order, float ripple_db) {

    if (ripple_db <= 0) {
        throw std::invalid_argument("Ripple must be > 0 dB");
    }

    auto s_poles = chebyshevPoles(order, ripple_db);
    double wc0 = prewarpFrequency(center_freq, sample_rate);
    double bw_rad = freqToRadians(bandwidth);

    auto z_poles_temp = transformToBandstop(s_poles, wc0, bw_rad);

    std::vector<std::complex<double>> z_poles;
    for (const auto& s : z_poles_temp) {
        z_poles.push_back(bilinearTransform(s, sample_rate));
    }

    std::vector<FilterPtr> biquads;
    for (size_t i = 0; i < z_poles.size(); i += 2) {
        auto biquad = std::make_unique<BiquadFilter>();
        biquad->configure(sample_rate, 1);

        if (i + 1 < z_poles.size()) {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i + 1], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        } else {
            float b0, b1, b2, a1, a2;
            poleToBiquad(z_poles[i], z_poles[i], b0, b1, b2, a1, a2);
            biquad->setCoefficients(b0, b1, b2, a1, a2);
        }

        biquads.push_back(std::move(biquad));
    }

    return biquads;
}

// ===== Helper Functions =====

std::vector<std::complex<double>> IIRDesigner::butterworthPoles(size_t order) {
    std::vector<std::complex<double>> poles;

    for (size_t k = 0; k < order; ++k) {
        // pole_k = exp(j*π*(2k+1)/(2N))
        double angle = M_PI * (2.0 * k + 1.0) / (2.0 * order);
        poles.emplace_back(std::cos(angle), std::sin(angle));
    }

    return poles;
}

std::vector<std::complex<double>> IIRDesigner::chebyshevPoles(
    size_t order, float ripple_db) {

    std::vector<std::complex<double>> poles;

    // Ripple factor δ: passband ripple = 20*log10(1/sqrt(1+δ²))
    // Given ripple_db: δ = sqrt(10^(ripple_db/10) - 1)
    double ripple_linear = std::pow(10.0, ripple_db / 10.0);
    double delta = std::sqrt(ripple_linear - 1.0);

    // Parameter α = sinh^-1(1/δ) / N
    double alpha = std::asinh(1.0 / delta) / order;

    // Poles:
    // pole_k = -sinh(α)*sin(θ_k) + j*cosh(α)*cos(θ_k)
    // where θ_k = π*(2k+1)/(2N)
    double sinh_alpha = std::sinh(alpha);
    double cosh_alpha = std::cosh(alpha);

    for (size_t k = 0; k < order; ++k) {
        double theta = M_PI * (2.0 * k + 1.0) / (2.0 * order);
        double real_part = -sinh_alpha * std::sin(theta);
        double imag_part = cosh_alpha * std::cos(theta);
        poles.emplace_back(real_part, imag_part);
    }

    return poles;
}

std::complex<double> IIRDesigner::bilinearTransform(
    std::complex<double> s_pole, uint32_t sample_rate) {

    double Ts = 1.0 / sample_rate;
    double half_Ts = Ts / 2.0;

    // z = (1 + s*Ts/2) / (1 - s*Ts/2)
    std::complex<double> numerator = 1.0 + s_pole * half_Ts;
    std::complex<double> denominator = 1.0 - s_pole * half_Ts;

    return numerator / denominator;
}

void IIRDesigner::poleToBiquad(std::complex<double> pole1,
                              std::complex<double> pole2,
                              float& b0, float& b1, float& b2,
                              float& a1, float& a2) {
    // For simplicity, set b0=1, b1=0, b2=0 (all-pass denominator only)
    // This assumes zeros are at z=1 (appropriate for lowpass)

    b0 = 1.0f;
    b1 = 0.0f;
    b2 = 0.0f;

    if (std::abs(pole1.imag()) < 1e-10) {
        // Real pole
        a1 = -2.0f * static_cast<float>(pole1.real());
        a2 = 0.0f;
    } else {
        // Complex conjugate pair
        a1 = -2.0f * static_cast<float>(pole1.real());
        a2 = static_cast<float>(std::norm(pole1));
    }
}

void IIRDesigner::transformToHighpass(
    std::vector<std::complex<double>>& poles, double wc) {

    // Transform s → wc/s
    // For a pole s_k: new_pole = wc / s_k
    for (auto& pole : poles) {
        pole = wc / pole;
    }
}

std::vector<std::complex<double>> IIRDesigner::transformToBandpass(
    const std::vector<std::complex<double>>& input_poles,
    double wc0, double bandwidth) {

    std::vector<std::complex<double>> output_poles;

    // Transform s → (s² + wc0²) / (s*BW)
    // Each pole s_k splits into 2 poles from: s*BW*pole_new = s² + wc0²
    // This is quadratic: pole_new² + (BW/pole_new)*pole_new + wc0² = 0

    for (const auto& sk : input_poles) {
        // Quadratic: x² - (BW/sk)*x + wc0² = 0
        // Using quadratic formula...
        // For now, simple transformation: approximate by pole shifting
        double BW_over_sk_real = bandwidth / sk.real();
        double discriminant = BW_over_sk_real * BW_over_sk_real - 4.0 * wc0 * wc0;

        std::complex<double> sqrt_term;
        if (discriminant >= 0) {
            sqrt_term = std::sqrt(discriminant);
        } else {
            sqrt_term = std::complex<double>(0, std::sqrt(-discriminant));
        }

        std::complex<double> pole1 = (BW_over_sk_real + sqrt_term) / 2.0;
        std::complex<double> pole2 = (BW_over_sk_real - sqrt_term) / 2.0;

        output_poles.push_back(pole1);
        output_poles.push_back(pole2);
    }

    return output_poles;
}

std::vector<std::complex<double>> IIRDesigner::transformToBandstop(
    const std::vector<std::complex<double>>& input_poles,
    double wc0, double bandwidth) {

    std::vector<std::complex<double>> output_poles;

    // Transform s → (s*BW) / (s² + wc0²)
    // Each pole s_k transforms similarly to bandpass
    for (const auto& sk : input_poles) {
        double BW_sk_real = bandwidth / sk.real();
        double discriminant = BW_sk_real * BW_sk_real - 4.0 * wc0 * wc0;

        std::complex<double> sqrt_term;
        if (discriminant >= 0) {
            sqrt_term = std::sqrt(discriminant);
        } else {
            sqrt_term = std::complex<double>(0, std::sqrt(-discriminant));
        }

        std::complex<double> pole1 = (BW_sk_real + sqrt_term) / 2.0;
        std::complex<double> pole2 = (BW_sk_real - sqrt_term) / 2.0;

        output_poles.push_back(pole1);
        output_poles.push_back(pole2);
    }

    return output_poles;
}

double IIRDesigner::prewarpFrequency(float fc_digital, uint32_t sample_rate) {
    // ωc_analog = 2/Ts * tan(π * fc_digital / fs)
    double fs = static_cast<double>(sample_rate);
    double f_norm = fc_digital / fs;
    double omega_prewarp = (2.0 * fs) * std::tan(M_PI * f_norm);
    return omega_prewarp;
}

}  // namespace audiofilter
