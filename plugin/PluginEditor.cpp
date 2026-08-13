#include "PluginEditor.h"

namespace
{
    // Mirrors the AudioParameterFloat's NormalisableRange onto a Slider so
    // dragging feels the same as the parameter's own skew (log-ish for
    // frequency/bandwidth). ComboBoxAttachment/SliderAttachment sync values
    // but do not copy the range across on their own.
    void matchSliderRangeToParameter(juce::Slider& slider,
                                      juce::AudioProcessorValueTreeState& apvts,
                                      const juce::String& paramId)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(paramId)))
        {
            slider.setNormalisableRange({ (double) p->range.start, (double) p->range.end,
                                           (double) p->range.interval, (double) p->range.skew });
        }
    }

    void setUpLabel(juce::Label& label, juce::Component& owner)
    {
        label.setJustificationType(juce::Justification::centred);
        owner.addAndMakeVisible(label);
    }
}

AudioFilterLibAudioProcessorEditor::AudioFilterLibAudioProcessorEditor(AudioFilterLibAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    titleLabel.setText("AudioFilterLib Demo", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    addAndMakeVisible(titleLabel);

    // --- Filter type ---
    filterTypeBox.addItemList({ "Lowpass", "Highpass", "Bandpass", "Bandstop" }, 1);
    addAndMakeVisible(filterTypeBox);
    setUpLabel(filterTypeLabel, *this);
    filterTypeAttachment = std::make_unique<ComboAttachment>(
        processor.apvts, AudioFilterLibAudioProcessor::filterTypeParamId, filterTypeBox);
    filterTypeBox.onChange = [this] { updateEnablement(); };

    // --- Design method ---
    designMethodBox.addItemList({ "Butterworth", "Chebyshev I" }, 1);
    addAndMakeVisible(designMethodBox);
    setUpLabel(designMethodLabel, *this);
    designMethodAttachment = std::make_unique<ComboAttachment>(
        processor.apvts, AudioFilterLibAudioProcessor::designMethodParamId, designMethodBox);
    designMethodBox.onChange = [this] { updateEnablement(); };

    // --- Order ---
    orderBox.addItemList({ "2", "4", "6", "8" }, 1);
    addAndMakeVisible(orderBox);
    setUpLabel(orderLabel, *this);
    orderAttachment = std::make_unique<ComboAttachment>(
        processor.apvts, AudioFilterLibAudioProcessor::orderParamId, orderBox);

    // --- Frequency / bandwidth / ripple rotary sliders ---
    for (auto* slider : { &frequencySlider, &bandwidthSlider, &rippleSlider })
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        addAndMakeVisible(slider);
    }
    setUpLabel(frequencyLabel, *this);
    setUpLabel(bandwidthLabel, *this);
    setUpLabel(rippleLabel, *this);

    matchSliderRangeToParameter(frequencySlider, processor.apvts, AudioFilterLibAudioProcessor::frequencyParamId);
    matchSliderRangeToParameter(bandwidthSlider, processor.apvts, AudioFilterLibAudioProcessor::bandwidthParamId);
    matchSliderRangeToParameter(rippleSlider, processor.apvts, AudioFilterLibAudioProcessor::rippleParamId);

    frequencyAttachment = std::make_unique<SliderAttachment>(
        processor.apvts, AudioFilterLibAudioProcessor::frequencyParamId, frequencySlider);
    bandwidthAttachment = std::make_unique<SliderAttachment>(
        processor.apvts, AudioFilterLibAudioProcessor::bandwidthParamId, bandwidthSlider);
    rippleAttachment = std::make_unique<SliderAttachment>(
        processor.apvts, AudioFilterLibAudioProcessor::rippleParamId, rippleSlider);

    updateEnablement();
    setSize(520, 320);
}

void AudioFilterLibAudioProcessorEditor::updateEnablement()
{
    const bool isBandType = filterTypeBox.getSelectedItemIndex() >= 2;   // Bandpass, Bandstop
    const bool isChebyshev = designMethodBox.getSelectedItemIndex() == 1; // Chebyshev I

    bandwidthSlider.setEnabled(isBandType);
    bandwidthLabel.setEnabled(isBandType);

    rippleSlider.setEnabled(isChebyshev);
    rippleLabel.setEnabled(isChebyshev);
}

void AudioFilterLibAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void AudioFilterLibAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(12);

    auto comboRow = area.removeFromTop(56);
    const int comboWidth = comboRow.getWidth() / 3;
    for (auto pair : { std::pair { &filterTypeLabel, &filterTypeBox },
                         std::pair { &designMethodLabel, &designMethodBox },
                         std::pair { &orderLabel, &orderBox } })
    {
        auto col = comboRow.removeFromLeft(comboWidth).reduced(6, 0);
        pair.first->setBounds(col.removeFromTop(18));
        pair.second->setBounds(col.removeFromTop(28));
    }

    area.removeFromTop(20);

    auto knobRow = area.removeFromTop(140);
    const int knobWidth = knobRow.getWidth() / 3;
    for (auto* pair : { std::pair { &frequencyLabel, &frequencySlider },
                         std::pair { &bandwidthLabel, &bandwidthSlider },
                         std::pair { &rippleLabel, &rippleSlider } })
    {
        auto col = knobRow.removeFromLeft(knobWidth).reduced(6, 0);
        pair.first->setBounds(col.removeFromTop(18));
        pair.second->setBounds(col);
    }
}
