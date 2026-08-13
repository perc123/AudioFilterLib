#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "iir_designer.h"
#include "filter_base.h"

/**
 * @class AudioFilterLibAudioProcessor
 * @brief A minimal JUCE AudioProcessor that wraps AudioFilterLib's
 *        IIRDesigner/BiquadFilter classes.
 *
 * Exposes filter type, design method, cutoff/center frequency, bandwidth,
 * order and ripple as automatable JUCE parameters (via
 * juce::AudioProcessorValueTreeState) and (re)designs a cascade of
 * audiofilter::BiquadFilter stages -- one independent cascade per channel,
 * since each BiquadFilter holds its own delay-line state -- whenever a
 * parameter changes.
 *
 * **Learning-exercise simplification:** each processBlock() call compares
 * the current parameter values against a cached snapshot and, on a change,
 * redesigns the filter chain right there on the audio thread.
 * audiofilter::IIRDesigner allocates (std::vector, std::unique_ptr per
 * biquad), so this is *not* strictly real-time-safe -- a production plugin
 * would redesign on a background thread and hand the new filter chain to
 * the audio thread via a lock-free swap. Left simple here on purpose; see
 * README.md's "Known limitations" for the fix.
 */
class AudioFilterLibAudioProcessor : public juce::AudioProcessor
{
public:
    AudioFilterLibAudioProcessor();
    ~AudioFilterLibAudioProcessor() override = default;

    // ===== AudioProcessor overrides =====
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Parameter IDs, shared with the editor.
    static constexpr auto filterTypeParamId   = "filterType";
    static constexpr auto designMethodParamId = "designMethod";
    static constexpr auto frequencyParamId    = "frequency";
    static constexpr auto bandwidthParamId    = "bandwidth";
    static constexpr auto orderParamId        = "order";
    static constexpr auto rippleParamId       = "ripple";

private:
    // true if the live parameter values differ from the cached snapshot
    // used to build the current channelFilters (or no chain exists yet).
    bool parametersHaveChanged() const;
    void rebuildFilters();
    std::vector<audiofilter::FilterPtr> designChain(
        audiofilter::FilterType type, audiofilter::DesignMethod method,
        uint32_t sampleRate, float frequency, float bandwidth,
        size_t order, float rippleDb);

    audiofilter::IIRDesigner designer;

    // One independent cascade of biquads per channel (each BiquadFilter
    // instance owns its own delay-line state, so channels can't share one).
    std::vector<std::vector<audiofilter::FilterPtr>> channelFilters;

    // Snapshot of the parameter values (+ sample rate) channelFilters was
    // last built from; compared against current values each processBlock().
    struct Snapshot
    {
        int filterTypeIndex = -1;
        int designMethodIndex = -1;
        float frequency = -1.0f;
        float bandwidth = -1.0f;
        int orderIndex = -1;
        float rippleDb = -1.0f;
        double sampleRate = -1.0;
    } lastBuilt;

    double currentSampleRate = audiofilter::FilterBase::DEFAULT_SAMPLE_RATE;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFilterLibAudioProcessor)
};
