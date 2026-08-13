# === JUCE demo plugin: wraps audiofilter's IIRDesigner/BiquadFilter in a
#     real JUCE AudioProcessor. See plugin/README.md for what it is.

juce_add_plugin(audiofilter_plugin
    COMPANY_NAME "AudioFilterLib"
    PRODUCT_NAME "AudioFilterLib Demo"
    PLUGIN_MANUFACTURER_CODE Afib
    PLUGIN_CODE Af01
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Fx" "Filter"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

target_sources(audiofilter_plugin
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
)

target_compile_definitions(audiofilter_plugin
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(audiofilter_plugin
    PRIVATE
        audiofilter
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
