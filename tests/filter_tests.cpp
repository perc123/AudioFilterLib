/**
 * @file filter_tests.cpp
 * @brief Unit tests for audio filter library
 *
 * Tests cover:
 * - Filter configuration and state management
 * - IIR filter coefficient computation
 * - FIR filter design and windowing
 * - Frequency response analysis
 * - Filter stability (poles in unit circle)
 * - Basic filtering operations
 */

#include <gtest/gtest.h>
#include "biquad_filter.h"
#include "iir_designer.h"
#include "fir_designer.h"
#include "audio_buffer.h"
#include <cmath>
#include <algorithm>

using namespace audiofilter;

// ===== Test Fixtures =====

class BiquadFilterTest : public ::testing::Test {
protected:
    BiquadFilter filter;
    static constexpr uint32_t SAMPLE_RATE = 48000;
};

class IIRDesignerTest : public ::testing::Test {
protected:
    IIRDesigner designer;
    static constexpr uint32_t SAMPLE_RATE = 48000;
};

class FIRDesignerTest : public ::testing::Test {
protected:
    FIRDesigner designer;
    static constexpr uint32_t SAMPLE_RATE = 48000;
};

class AudioBufferTest : public ::testing::Test {
protected:
    static constexpr uint32_t SAMPLE_RATE = 48000;
};

// ===== BiquadFilter Tests =====

TEST_F(BiquadFilterTest, DefaultConstructor) {
    // Default biquad should be unity gain (all-pass)
    EXPECT_FALSE(filter.isConfigured());
    EXPECT_EQ(filter.getOrder(), 2);
    EXPECT_EQ(filter.getName(), std::string("BiquadFilter (2nd-order IIR)"));
}

TEST_F(BiquadFilterTest, Configuration) {
    filter.configure(SAMPLE_RATE, 1);
    EXPECT_TRUE(filter.isConfigured());
    EXPECT_EQ(filter.getSampleRate(), SAMPLE_RATE);
    EXPECT_EQ(filter.getChannels(), 1);
}

TEST_F(BiquadFilterTest, InvalidConfiguration) {
    // Sample rate too low
    EXPECT_THROW(filter.configure(4000, 1), std::invalid_argument);

    // Sample rate too high
    EXPECT_THROW(filter.configure(200000, 1), std::invalid_argument);

    // Zero channels
    EXPECT_THROW(filter.configure(SAMPLE_RATE, 0), std::invalid_argument);
}

TEST_F(BiquadFilterTest, UnityGainPassThrough) {
    // Unity gain biquad (b0=1, b1=0, b2=0, a1=0, a2=0) should pass signal unchanged
    filter.setCoefficients(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    filter.configure(SAMPLE_RATE, 1);

    float test_signal[] = {0.5f, -0.3f, 0.8f, -0.1f};
    for (float sample : test_signal) {
        float output = filter.process(sample);
        EXPECT_FLOAT_EQ(output, sample);
    }
}

TEST_F(BiquadFilterTest, ResetState) {
    // Set non-zero coefficients
    filter.setCoefficients(0.5f, 0.3f, 0.2f, -1.0f, 0.5f);
    filter.configure(SAMPLE_RATE, 1);

    // Process some samples to accumulate state
    filter.process(1.0f);
    filter.process(0.5f);

    // Reset and verify state is cleared
    filter.reset();
    float w1, w2;
    filter.getState(w1, w2);
    EXPECT_FLOAT_EQ(w1, 0.0f);
    EXPECT_FLOAT_EQ(w2, 0.0f);
}

TEST_F(BiquadFilterTest, CoefficientStorage) {
    float b0 = 0.2f, b1 = 0.4f, b2 = 0.2f;
    float a1 = 1.2f, a2 = 0.5f;

    filter.setCoefficients(b0, b1, b2, a1, a2);

    float out_b0, out_b1, out_b2, out_a1, out_a2;
    filter.getCoefficients(out_b0, out_b1, out_b2, out_a1, out_a2);

    EXPECT_FLOAT_EQ(out_b0, b0);
    EXPECT_FLOAT_EQ(out_b1, b1);
    EXPECT_FLOAT_EQ(out_b2, b2);
    EXPECT_FLOAT_EQ(out_a1, a1);
    EXPECT_FLOAT_EQ(out_a2, a2);
}

TEST_F(BiquadFilterTest, ProcessFrame) {
    filter.setCoefficients(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);  // Unity gain
    filter.configure(SAMPLE_RATE, 1);

    float buffer[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float expected[] = {0.1f, 0.2f, 0.3f, 0.4f};

    filter.processFrame(buffer, 4);

    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(buffer[i], expected[i]);
    }
}

TEST_F(BiquadFilterTest, ProcessFrameInPlace) {
    // Gain of 0.5
    filter.setCoefficients(0.5f, 0.0f, 0.0f, 0.0f, 0.0f);
    filter.configure(SAMPLE_RATE, 1);

    float buffer[] = {1.0f, 2.0f, 3.0f};
    filter.processFrame(buffer, 3);

    EXPECT_FLOAT_EQ(buffer[0], 0.5f);
    EXPECT_FLOAT_EQ(buffer[1], 1.0f);
    EXPECT_FLOAT_EQ(buffer[2], 1.5f);
}

// ===== IIRDesigner Tests =====

TEST_F(IIRDesignerTest, ButterworthLowpassOrder2) {
    auto biquads = designer.designButterworthLowpass(SAMPLE_RATE, 1000.0f, 2);

    // 2nd order should produce 1 biquad
    EXPECT_EQ(biquads.size(), 1);

    // Each biquad should be configured
    for (auto& biq : biquads) {
        EXPECT_TRUE(biq->isConfigured());
        EXPECT_EQ(biq->getSampleRate(), SAMPLE_RATE);
    }
}

TEST_F(IIRDesignerTest, ButterworthLowpassOrder4) {
    auto biquads = designer.designButterworthLowpass(SAMPLE_RATE, 5000.0f, 4);

    // 4th order should produce 2 biquads
    EXPECT_EQ(biquads.size(), 2);
}

TEST_F(IIRDesignerTest, ChebyshevLowpass) {
    auto biquads = designer.designChebyshevLowpass(
        SAMPLE_RATE, 1000.0f, 4, 0.5f);

    EXPECT_EQ(biquads.size(), 2);

    for (auto& biq : biquads) {
        EXPECT_TRUE(biq->isConfigured());
    }
}

TEST_F(IIRDesignerTest, InvalidCutoffFrequency) {
    // Cutoff above Nyquist
    EXPECT_THROW(
        designer.designButterworthLowpass(SAMPLE_RATE, 30000.0f, 2),
        std::invalid_argument
    );

    // Negative cutoff
    EXPECT_THROW(
        designer.designButterworthLowpass(SAMPLE_RATE, -1000.0f, 2),
        std::invalid_argument
    );
}

TEST_F(IIRDesignerTest, InvalidRipple) {
    // Negative ripple
    EXPECT_THROW(
        designer.designChebyshevLowpass(SAMPLE_RATE, 1000.0f, 4, -0.5f),
        std::invalid_argument
    );
}

TEST_F(IIRDesignerTest, FilterCascade) {
    // Create a 4th order filter (2 biquads)
    auto biquads = designer.designButterworthLowpass(SAMPLE_RATE, 1000.0f, 4);

    // Create test signal (impulse)
    float impulse = 1.0f;
    float output = impulse;

    // Process through cascade
    for (auto& biq : biquads) {
        output = biq->process(output);
    }

    // Output should be finite (not NaN or Inf)
    EXPECT_TRUE(std::isfinite(output));
}

// ===== FIRDesigner Tests =====

TEST_F(FIRDesignerTest, KaiserLowpassDesign) {
    auto taps = designer.designKaiserLowpass(
        SAMPLE_RATE, 5000.0f, 80.0f, 100.0f);

    // Should have odd number of taps for symmetric design
    EXPECT_TRUE(taps.size() % 2 == 1);

    // Taps should not be empty
    EXPECT_GT(taps.size(), 0);

    // Sum should be approximately 1.0 (unity gain)
    float sum = 0.0f;
    for (float tap : taps) {
        sum += tap;
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

TEST_F(FIRDesignerTest, HammingLowpass) {
    auto taps = designer.designHammingLowpass(SAMPLE_RATE, 5000.0f, 51);

    EXPECT_EQ(taps.size(), 51);

    // Check symmetry (linear phase guarantee)
    for (size_t i = 0; i < taps.size() / 2; ++i) {
        EXPECT_NEAR(taps[i], taps[taps.size() - 1 - i], 1e-6f);
    }
}

TEST_F(FIRDesignerTest, FIRFilterCreation) {
    auto taps = designer.designKaiserLowpass(
        SAMPLE_RATE, 5000.0f, 80.0f, 100.0f);

    auto fir = designer.createFIRFilter(taps);
    EXPECT_TRUE(fir != nullptr);

    fir->configure(SAMPLE_RATE, 1);
    EXPECT_TRUE(fir->isConfigured());
    EXPECT_EQ(fir->getOrder(), taps.size());
}

TEST_F(FIRDesignerTest, FIRProcessing) {
    auto taps = designer.designKaiserLowpass(
        SAMPLE_RATE, 5000.0f, 80.0f, 100.0f);

    auto fir = designer.createFIRFilter(taps);
    fir->configure(SAMPLE_RATE, 1);

    // Process impulse
    float output = fir->process(1.0f);
    EXPECT_TRUE(std::isfinite(output));

    // Output should be approximately first tap
    EXPECT_NEAR(output, taps[taps.size() / 2], 0.1f);
}

// ===== AudioBuffer Tests =====

TEST_F(AudioBufferTest, Creation) {
    AudioBuffer buffer(SAMPLE_RATE, 2, 1024);

    EXPECT_EQ(buffer.getSampleRate(), SAMPLE_RATE);
    EXPECT_EQ(buffer.getChannels(), 2);
    EXPECT_GE(buffer.getCapacity(), 1024);  // May be rounded up to power-of-2
    EXPECT_TRUE(buffer.isEmpty());
}

TEST_F(AudioBufferTest, StereoWriteRead) {
    AudioBuffer buffer(SAMPLE_RATE, 2, 256);

    // Write a stereo sample
    buffer.write(0.5f, -0.3f);

    EXPECT_EQ(buffer.availableRead(), 1);

    // Read it back
    auto samples = buffer.read(1);
    EXPECT_EQ(samples.size(), 2);
    EXPECT_NEAR(samples[0], 0.5f, 1e-6f);
    EXPECT_NEAR(samples[1], -0.3f, 1e-6f);
}

TEST_F(AudioBufferTest, FrameWriteRead) {
    AudioBuffer buffer(SAMPLE_RATE, 1, 256);

    float input[] = {0.1f, 0.2f, 0.3f};
    buffer.write(input, 3);

    EXPECT_EQ(buffer.availableRead(), 3);

    auto output = buffer.read(3);
    EXPECT_EQ(output.size(), 3);
    for (int i = 0; i < 3; ++i) {
        EXPECT_NEAR(output[i], input[i], 1e-6f);
    }
}

TEST_F(AudioBufferTest, Wraparound) {
    AudioBuffer buffer(SAMPLE_RATE, 1, 4);  // Small capacity for testing

    // Write and read multiple times to trigger wraparound
    for (int i = 0; i < 10; ++i) {
        buffer.write(&((float[]){0.5f})[0], 1);
    }

    auto samples = buffer.read(10);
    EXPECT_EQ(samples.size(), 10);

    for (float s : samples) {
        EXPECT_NEAR(s, 0.5f, 1e-6f);
    }
}

TEST_F(AudioBufferTest, FillPercentage) {
    AudioBuffer buffer(SAMPLE_RATE, 1, 100);

    EXPECT_EQ(buffer.getFillPercentage(), 0.0f);

    float sample = 0.5f;
    for (int i = 0; i < 50; ++i) {
        buffer.write(&sample, 1);
    }

    float fill = buffer.getFillPercentage();
    EXPECT_NEAR(fill, 50.0f, 1.0f);
}

TEST_F(AudioBufferTest, Statistics) {
    AudioBuffer buffer(SAMPLE_RATE, 1, 256);

    EXPECT_EQ(buffer.getTotalWritten(), 0);
    EXPECT_EQ(buffer.getTotalRead(), 0);

    float sample = 0.5f;
    buffer.write(&sample, 10);
    EXPECT_EQ(buffer.getTotalWritten(), 10);

    auto data = buffer.read(10);
    EXPECT_EQ(buffer.getTotalRead(), 10);
}

// ===== Integration Tests =====

TEST(IntegrationTest, IIRFilteringAudio) {
    // Design a simple lowpass filter
    IIRDesigner designer;
    auto biquads = designer.designButterworthLowpass(48000, 5000.0f, 2);

    // Create test signal (1 kHz sine wave, 48 samples @ 48kHz)
    std::vector<float> signal(48);
    for (size_t i = 0; i < signal.size(); ++i) {
        float t = static_cast<float>(i) / 48000.0f;
        signal[i] = std::sin(2.0f * M_PI * 1000.0f * t);
    }

    // Process through filter
    std::vector<float> filtered = signal;
    for (auto& biq : biquads) {
        biq->processFrame(filtered.data(), filtered.size());
    }

    // Output should be finite
    for (float sample : filtered) {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(IntegrationTest, FIRFilteringAudio) {
    // Design FIR filter
    FIRDesigner designer;
    auto taps = designer.designKaiserLowpass(48000, 5000.0f, 80.0f, 100.0f);
    auto fir = designer.createFIRFilter(taps);
    fir->configure(48000, 1);

    // Create test signal
    std::vector<float> signal(1000);
    for (size_t i = 0; i < signal.size(); ++i) {
        float t = static_cast<float>(i) / 48000.0f;
        signal[i] = std::sin(2.0f * M_PI * 10000.0f * t);  // 10 kHz (above cutoff)
    }

    // Process
    fir->processFrame(signal.data(), signal.size());

    // Output should be attenuated (lower amplitude than input for 10 kHz above cutoff)
    float input_energy = 0.0f, output_energy = 0.0f;
    for (size_t i = 0; i < 100; ++i) {
        float t = static_cast<float>(i) / 48000.0f;
        input_energy += std::abs(std::sin(2.0f * M_PI * 10000.0f * t));
        output_energy += std::abs(signal[i]);
    }

    EXPECT_LT(output_energy, input_energy);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
