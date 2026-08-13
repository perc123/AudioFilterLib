#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace audiofilter;

namespace
{
    // Index <-> audiofilter::FilterType / DesignMethod, and index <-> filter
    // order, all in the same order the AudioParameterChoice lists below use.
    constexpr FilterType kFilterTypes[] = {
        FilterType::Lowpass, FilterType::Highpass,
        FilterType::Bandpass, FilterType::Bandstop
    };
    constexpr DesignMethod kDesignMethods[] = {
        DesignMethod::Butterworth, DesignMethod::Chebyshev1
    };
    constexpr size_t kOrders[] = { 2, 4, 6, 8 };
}

AudioFilterLibAudioProcessor::AudioFilterLibAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
AudioFilterLibAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { filterTypeParamId, 1 }, "Filter Type",
        juce::StringArray { "Lowpass", "Highpass", "Bandpass", "Bandstop" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { designMethodParamId, 1 }, "Design Method",
        juce::StringArray { "Butterworth", "Chebyshev I" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { frequencyParamId, 1 }, "Frequency",
        juce::NormalisableRange<float> { 20.0f, 20000.0f, 1.0f, 0.3f }, 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { bandwidthParamId, 1 }, "Bandwidth",
        juce::NormalisableRange<float> { 10.0f, 10000.0f, 1.0f, 0.3f }, 200.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { orderParamId, 1 }, "Order",
        juce::StringArray { "2", "4", "6", "8" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { rippleParamId, 1 }, "Ripple",
        juce::NormalisableRange<float> { 0.1f, 3.0f, 0.01f }, 0.5f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}

void AudioFilterLibAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    lastBuilt.sampleRate = -1.0;  // force a rebuild on the next processBlock()
}

bool AudioFilterLibAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto set = layouts.getMainOutputChannelSet();
    if (set != juce::AudioChannelSet::mono() && set != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == set;
}

std::vector<FilterPtr> AudioFilterLibAudioProcessor::designChain(
    FilterType type, DesignMethod method, uint32_t sampleRate,
    float frequency, float bandwidth, size_t order, float rippleDb)
{
    const bool chebyshev = (method == DesignMethod::Chebyshev1);

    switch (type)
    {
        case FilterType::Lowpass:
            return chebyshev
                ? designer.designChebyshevLowpass(sampleRate, frequency, order, rippleDb)
                : designer.designButterworthLowpass(sampleRate, frequency, order);

        case FilterType::Highpass:
            return chebyshev
                ? designer.designChebyshevHighpass(sampleRate, frequency, order, rippleDb)
                : designer.designButterworthHighpass(sampleRate, frequency, order);

        case FilterType::Bandpass:
            return chebyshev
                ? designer.designChebyshevBandpass(sampleRate, frequency, bandwidth, order, rippleDb)
                : designer.designButterworthBandpass(sampleRate, frequency, bandwidth, order);

        case FilterType::Bandstop:
        default:
            return chebyshev
                ? designer.designChebyshevBandstop(sampleRate, frequency, bandwidth, order, rippleDb)
                : designer.designButterworthBandstop(sampleRate, frequency, bandwidth, order);
    }
}

void AudioFilterLibAudioProcessor::rebuildFilters()
{
    const auto sr = (uint32_t) juce::jlimit(8000.0, 192000.0, currentSampleRate);
    const auto numChannels = (size_t) juce::jmax(1, getTotalNumOutputChannels());

    const auto typeIndex = (size_t) apvts.getRawParameterValue(filterTypeParamId)->load();
    const auto methodIndex = (size_t) apvts.getRawParameterValue(designMethodParamId)->load();
    const auto frequency = apvts.getRawParameterValue(frequencyParamId)->load();
    const auto bandwidth = apvts.getRawParameterValue(bandwidthParamId)->load();
    const auto orderIndex = (size_t) apvts.getRawParameterValue(orderParamId)->load();
    const auto rippleDb = apvts.getRawParameterValue(rippleParamId)->load();

    const auto type = kFilterTypes[juce::jmin(typeIndex, std::size(kFilterTypes) - 1)];
    const auto method = kDesignMethods[juce::jmin(methodIndex, std::size(kDesignMethods) - 1)];
    const auto order = kOrders[juce::jmin(orderIndex, std::size(kOrders) - 1)];

    // Clamp center/cutoff so it can't cross Nyquist as sample rate changes.
    const float safeFrequency = juce::jmin(frequency, (float) sr * 0.45f);

    channelFilters.clear();
    channelFilters.resize(numChannels);

    for (auto& chain : channelFilters)
    {
        chain = designChain(type, method, sr, safeFrequency, bandwidth, order, rippleDb);
        for (auto& biquad : chain)
            biquad->configure(sr, 1);
    }
}

void AudioFilterLibAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (parametersChanged.exchange(false))
        rebuildFilters();

    const auto numSamples = (size_t) buffer.getNumSamples();
    const auto numChannels = juce::jmin(buffer.getNumChannels(), (int) channelFilters.size());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (auto& biquad : channelFilters[(size_t) ch])
            biquad->processFrame(data, numSamples);
    }
}

juce::AudioProcessorEditor* AudioFilterLibAudioProcessor::createEditor()
{
    return new AudioFilterLibAudioProcessorEditor(*this);
}

void AudioFilterLibAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AudioFilterLibAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));

    parametersChanged = true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFilterLibAudioProcessor();
}
