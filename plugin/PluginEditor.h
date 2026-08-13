#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

/**
 * @class AudioFilterLibAudioProcessorEditor
 * @brief Minimal GUI for AudioFilterLibAudioProcessor.
 *
 * One combo box each for filter type / design method / order, and one
 * rotary slider each for frequency / bandwidth / ripple, all bound to the
 * processor's AudioProcessorValueTreeState via Attachments (so the host's
 * automation, undo, and state save/load all keep working without any
 * manual wiring here).
 */
class AudioFilterLibAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AudioFilterLibAudioProcessorEditor(AudioFilterLibAudioProcessor&);
    ~AudioFilterLibAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void updateEnablement();

    // Named audioProcessor (not processor) to avoid shadowing
    // AudioProcessorEditor::processor (same reference, base class' type).
    AudioFilterLibAudioProcessor& audioProcessor;

    juce::Label titleLabel;

    juce::ComboBox filterTypeBox, designMethodBox, orderBox;
    juce::Label filterTypeLabel { {}, "Filter Type" }, designMethodLabel { {}, "Design Method" },
        orderLabel { {}, "Order" };

    juce::Slider frequencySlider, bandwidthSlider, rippleSlider;
    juce::Label frequencyLabel { {}, "Frequency" }, bandwidthLabel { {}, "Bandwidth" },
        rippleLabel { {}, "Ripple" };

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboAttachment> filterTypeAttachment, designMethodAttachment, orderAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, bandwidthAttachment, rippleAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFilterLibAudioProcessorEditor)
};
