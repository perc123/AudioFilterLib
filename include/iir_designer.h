#pragma once

#include "biquad_filter.h"
#include <vector>
#include <memory>
#include <complex>
#include <stdexcept>
#include <cmath>

/**
 * @file iir_designer.h
 * @brief IIR filter design algorithms (Butterworth, Chebyshev Type I)
 *
 * **Overview:**
 * Designs digital IIR filters by:
 * 1. Creating an analog prototype filter (s-plane)
 * 2. Applying the bilinear transform to convert to digital (z-plane)
 * 3. Converting resulting poles/zeros to biquad coefficients
 * 4. Returning a cascade of BiquadFilter stages
 *
 * **Design Steps:**
 *
 * **Step 1: Analog Prototype**
 * Classical analog filter designs have well-known pole locations.
 * For normalized cutoff ωc = 1 rad/s, the poles are:
 *
 * Butterworth (maximally flat passband):
 *   pole_k = exp(j*π*(2k+1)/(2N)) for k=0..N-1
 *   All poles lie on unit circle (|pole| = 1)
 *
 * Chebyshev Type I (equiripple passband):
 *   pole_k = -sinh(α/N)*sin(θ_k) + j*cosh(α/N)*cos(θ_k)
 *   where α = sinh^-1(1/δ), δ = ripple factor, θ_k = π*(2k+1)/(2N)
 *   Poles lie on ellipse (tighter = more ripple = steeper rolloff)
 *
 * **Step 2: Frequency Warping (Prewarping)**
 * The bilinear transform warps frequencies due to the arctangent mapping:
 *   ω_analog = 2/Ts * tan(π * ω_digital / fs)
 *
 * Without prewarping, the digital cutoff freq would be incorrect.
 * Solution: Prewarp the analog cutoff before bilinear transform:
 *   ωc_analog_prewarped = 2/Ts * tan(π * fc_digital / fs)
 *
 * Then scale analog poles: pole_digital = pole_analog * ωc_prewarped
 *
 * **Step 3: Bilinear Transform**
 * Maps s-plane (analog) → z-plane (digital):
 *   s = 2/Ts * (z - 1) / (z + 1)
 *
 * For each analog pole s_k:
 *   z_k = (1 + s_k*Ts/2) / (1 - s_k*Ts/2)
 *
 * Guarantees digital filter stability if analog poles are in LHP (Re(s) < 0).
 *
 * **Step 4: Pole-to-Biquad Conversion**
 * Complex conjugate pole pairs (p1, p1*) → one biquad:
 *   H(z) = K / [(1 - p1*z^-1)(1 - p1**z^-1)]
 *         = b0 / (1 + a1*z^-1 + a2*z^-2)
 *   where:
 *     a1 = -(p1 + p1*) = -2*Re(p1)
 *     a2 = p1 * p1* = |p1|^2
 *
 * **Topologies:**
 *
 * Lowpass (Fc = cutoff frequency):
 *   Use poles directly. DC gain ≈ 1.
 *
 * Highpass (Fc = cutoff frequency):
 *   Transform s → Wc/s (frequency inversion)
 *   Mirrors poles across imaginary axis
 *
 * Bandpass (Fc0 = center, BW = bandwidth):
 *   Transform s → (s² + Wc0²) / (s*BW)
 *   Doubles filter order (each pole → 2 poles)
 *
 * Bandstop/Notch (Fc0 = center, BW = bandwidth):
 *   Transform s → (s*BW) / (s² + Wc0²)
 *   Creates zeros at ±jWc0 (notch in stopband)
 *
 * **Example Usage:**
 *
 * ```cpp
 * IIRDesigner designer;
 *
 * // Design 4th-order Butterworth lowpass at 1 kHz, 48 kHz sample rate
 * auto biquads = designer.designButterworthLowpass(48000, 1000, 4);
 * // Returns vector of 2 BiquadFilter objects
 *
 * // Cascade them
 * for (auto& biq : biquads) {
 *     biq->configure(48000, 1);
 * }
 *
 * // Filter audio
 * float sample = input;
 * for (auto& biq : biquads) {
 *     sample = biq->process(sample);
 * }
 * ```
 */

namespace audiofilter {

/**
 * @enum FilterType
 * @brief Supported IIR filter topologies
 */
enum class FilterType {
    Lowpass,   ///< Attenuate frequencies above cutoff
    Highpass,  ///< Attenuate frequencies below cutoff
    Bandpass,  ///< Pass frequencies in a band
    Bandstop   ///< Reject frequencies in a band (notch filter)
};

/**
 * @enum DesignMethod
 * @brief Supported IIR design methods
 */
enum class DesignMethod {
    Butterworth,  ///< Maximally flat passband (slowest rolloff)
    Chebyshev1    ///< Equiripple passband (faster rolloff, more phase distortion)
};

/**
 * @struct PoleZeroPair
 * @brief A pole or zero in the s-plane (analog) or z-plane (digital)
 */
struct PoleZeroPair {
    std::complex<double> p1;  ///< First pole/zero
    std::complex<double> p2;  ///< Second pole/zero (conjugate if complex)
    bool is_complex;          ///< true if complex conjugate pair, false if real

    PoleZeroPair(std::complex<double> pole, std::complex<double> pole_conj = {})
        : p1(pole), p2(pole_conj),
          is_complex(pole_conj != std::complex<double>{}) {}
};

/**
 * @class IIRDesigner
 * @brief Designs IIR filters using analog prototypes + bilinear transform
 *
 * Implements the classical approach:
 * 1. Design normalized analog prototype (Butterworth, Chebyshev)
 * 2. Prewarp cutoff frequency
 * 3. Apply bilinear transform
 * 4. Convert poles to biquad coefficients
 */
class IIRDesigner {
public:
    // ===== Butterworth Filters =====

    /**
     * @brief Design a Butterworth lowpass filter
     *
     * Butterworth filters have a maximally flat passband (no ripple).
     * The magnitude response is monotonically decreasing.
     *
     * **Characteristics:**
     * - Passband: Maximally flat (gain ≈ 1 everywhere below cutoff)
     * - Transition: Gradual (-20 dB/decade/order)
     * - Phase: Relatively linear (good for audio)
     * - Order: N → N biquads (2 poles per biquad)
     *
     * @param sample_rate Sample rate in Hz (e.g., 48000)
     * @param cutoff_freq Cutoff frequency in Hz (e.g., 1000)
     * @param order Filter order (2, 4, 6, 8, ... typical values)
     *
     * @return Vector of configured BiquadFilter objects (size = ceil(order/2))
     *
     * @throw std::invalid_argument if parameters out of range
     *
     * **Example:**
     * ```cpp
     * IIRDesigner designer;
     * auto biquads = designer.designButterworthLowpass(48000, 5000, 4);
     * // Returns 2 biquads for 4th-order filter
     * ```
     */
    std::vector<FilterPtr> designButterworthLowpass(
        uint32_t sample_rate, float cutoff_freq, size_t order);

    /**
     * @brief Design a Butterworth highpass filter
     *
     * Inverse of lowpass: passes high frequencies, attenuates low frequencies.
     *
     * @param sample_rate Sample rate in Hz
     * @param cutoff_freq Cutoff frequency in Hz
     * @param order Filter order
     *
     * @return Vector of BiquadFilter objects
     *
     * @throw std::invalid_argument if parameters out of range
     */
    std::vector<FilterPtr> designButterworthHighpass(
        uint32_t sample_rate, float cutoff_freq, size_t order);

    /**
     * @brief Design a Butterworth bandpass filter
     *
     * Passes frequencies in a band, attenuates outside.
     *
     * **Note:** Bandpass is created by transforming lowpass, so effective
     * filter order doubles. E.g., order=2 lowpass → order=4 bandpass.
     *
     * @param sample_rate Sample rate in Hz
     * @param center_freq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz (width of passband)
     * @param order Original lowpass filter order (effective order = 2*order)
     *
     * @return Vector of BiquadFilter objects (size = order)
     *
     * @throw std::invalid_argument if parameters out of range
     */
    std::vector<FilterPtr> designButterworthBandpass(
        uint32_t sample_rate, float center_freq, float bandwidth, size_t order);

    /**
     * @brief Design a Butterworth bandstop (notch) filter
     *
     * Attenuates frequencies in a band, passes outside.
     * Useful for removing hum (60/50 Hz), interference, etc.
     *
     * @param sample_rate Sample rate in Hz
     * @param center_freq Center frequency of notch in Hz
     * @param bandwidth Notch width in Hz
     * @param order Original lowpass filter order (effective order = 2*order)
     *
     * @return Vector of BiquadFilter objects
     *
     * @throw std::invalid_argument if parameters out of range
     */
    std::vector<FilterPtr> designButterworthBandstop(
        uint32_t sample_rate, float center_freq, float bandwidth, size_t order);

    // ===== Chebyshev Type I Filters =====

    /**
     * @brief Design a Chebyshev Type I lowpass filter
     *
     * Chebyshev filters have equiripple passband for steeper rolloff than Butterworth.
     * Trade-off: passband ripple vs. rolloff sharpness.
     *
     * **Characteristics:**
     * - Passband: Equiripple (oscillates between 1 and 1-δ)
     * - Transition: Steeper than Butterworth for same order
     * - Phase: More distortion than Butterworth
     * - Ripple: Controlled by ripple_db parameter
     *
     * @param sample_rate Sample rate in Hz
     * @param cutoff_freq Cutoff frequency in Hz (-3dB point)
     * @param order Filter order
     * @param ripple_db Passband ripple in dB (e.g., 0.1, 0.5, 1.0)
     *              Smaller = flatter passband, less steep rolloff
     *              Larger = more ripple, steeper rolloff
     *
     * @return Vector of BiquadFilter objects
     *
     * @throw std::invalid_argument if ripple_db <= 0
     * @throw std::invalid_argument if other parameters out of range
     *
     * **Example:**
     * ```cpp
     * // 4th-order Chebyshev with 0.5 dB passband ripple
     * auto biquads = designer.designChebyshevLowpass(48000, 1000, 4, 0.5);
     * ```
     */
    std::vector<FilterPtr> designChebyshevLowpass(
        uint32_t sample_rate, float cutoff_freq, size_t order, float ripple_db);

    /**
     * @brief Design a Chebyshev Type I highpass filter
     *
     * @param sample_rate Sample rate in Hz
     * @param cutoff_freq Cutoff frequency in Hz
     * @param order Filter order
     * @param ripple_db Passband ripple in dB
     *
     * @return Vector of BiquadFilter objects
     */
    std::vector<FilterPtr> designChebyshevHighpass(
        uint32_t sample_rate, float cutoff_freq, size_t order, float ripple_db);

    /**
     * @brief Design a Chebyshev Type I bandpass filter
     *
     * @param sample_rate Sample rate in Hz
     * @param center_freq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param order Original lowpass filter order
     * @param ripple_db Passband ripple in dB
     *
     * @return Vector of BiquadFilter objects
     */
    std::vector<FilterPtr> designChebyshevBandpass(
        uint32_t sample_rate, float center_freq, float bandwidth,
        size_t order, float ripple_db);

    /**
     * @brief Design a Chebyshev Type I bandstop filter
     *
     * @param sample_rate Sample rate in Hz
     * @param center_freq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param order Original lowpass filter order
     * @param ripple_db Passband ripple in dB
     *
     * @return Vector of BiquadFilter objects
     */
    std::vector<FilterPtr> designChebyshevBandstop(
        uint32_t sample_rate, float center_freq, float bandwidth,
        size_t order, float ripple_db);

private:
    // ===== Private Helper Methods =====

    /**
     * @brief Compute poles for normalized analog Butterworth prototype (ωc = 1)
     *
     * @param order Filter order
     * @return Vector of poles as complex numbers
     */
    static std::vector<std::complex<double>> butterworthPoles(size_t order);

    /**
     * @brief Compute poles for normalized analog Chebyshev Type I prototype (ωc = 1)
     *
     * @param order Filter order
     * @param ripple_db Passband ripple in dB
     * @return Vector of poles as complex numbers
     */
    static std::vector<std::complex<double>> chebyshevPoles(size_t order, float ripple_db);

    /**
     * @brief Apply bilinear transform: s-plane → z-plane
     *
     * Maps analog pole s_k to digital pole z_k:
     *   z_k = (1 + s_k*Ts/2) / (1 - s_k*Ts/2)
     *
     * @param s_pole Analog pole
     * @param sample_rate Sample rate in Hz
     * @return Digital pole on z-plane
     */
    static std::complex<double> bilinearTransform(
        std::complex<double> s_pole, uint32_t sample_rate);

    /**
     * @brief Convert a pole pair to biquad coefficients
     *
     * For real pole p:
     *   H(z) = 1 / (1 - p*z^-1)  → a1 = -p, a2 = 0
     *
     * For complex conjugate pair (p, p*):
     *   H(z) = 1 / [(1 - p*z^-1)(1 - p**z^-1)]
     *   → a1 = -2*Re(p), a2 = |p|^2
     *
     * @param pole1 First pole
     * @param pole2 Second pole (conjugate if complex pair)
     * @param[out] b0, b1, b2 Feedforward coefficients
     * @param[out] a1, a2 Feedback coefficients
     */
    static void poleToBiquad(std::complex<double> pole1,
                            std::complex<double> pole2,
                            float& b0, float& b1, float& b2,
                            float& a1, float& a2);

    /**
     * @brief Apply lowpass-to-highpass transformation
     *
     * Transforms analog prototype poles using s → ωc/s
     *
     * @param[in,out] poles Vector of analog prototype poles
     * @param wc Cutoff frequency in rad/s
     */
    static void transformToHighpass(std::vector<std::complex<double>>& poles,
                                   double wc);

    /**
     * @brief Apply lowpass-to-bandpass transformation
     *
     * Transforms analog prototype using s → (s² + ωc0²) / (s*BW)
     * Doubles the number of poles.
     *
     * @param[in] input_poles Lowpass prototype poles
     * @param wc0 Center frequency in rad/s
     * @param bandwidth Bandwidth in rad/s
     * @return Bandpass poles (2 × input size)
     */
    static std::vector<std::complex<double>> transformToBandpass(
        const std::vector<std::complex<double>>& input_poles,
        double wc0, double bandwidth);

    /**
     * @brief Apply lowpass-to-bandstop transformation
     *
     * Transforms analog prototype using s → (s*BW) / (s² + ωc0²)
     *
     * @param[in] input_poles Lowpass prototype poles
     * @param wc0 Center frequency in rad/s
     * @param bandwidth Bandwidth in rad/s
     * @return Bandstop poles (2 × input size)
     */
    static std::vector<std::complex<double>> transformToBandstop(
        const std::vector<std::complex<double>>& input_poles,
        double wc0, double bandwidth);

    /**
     * @brief Prewarp cutoff frequency for bilinear transform
     *
     * Compensates for frequency warping: ω_analog = 2/Ts * tan(π*ω_digital/fs)
     *
     * @param fc_digital Digital cutoff frequency in Hz
     * @param sample_rate Sample rate in Hz
     * @return Prewarped analog cutoff frequency in rad/s
     */
    static double prewarpFrequency(float fc_digital, uint32_t sample_rate);

    /**
     * @brief Convert frequency in Hz to angular frequency in rad/s
     *
     * @param freq_hz Frequency in Hz
     * @return Angular frequency in rad/s (ω = 2π*f)
     */
    static double freqToRadians(float freq_hz) {
        return 2.0 * M_PI * freq_hz;
    }
};

}  // namespace audiofilter
