#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ArpExcelAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit ArpExcelAudioProcessorEditor(ArpExcelAudioProcessor&);
    ~ArpExcelAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ArpExcelAudioProcessor& processorRef;

    juce::ToggleButton enabledButton;
    juce::ComboBox modeCombo;
    juce::ComboBox rateCombo;
    juce::Slider numStepsSlider;
    juce::Slider gateSlider;
    juce::Slider octaveRangeSlider;
    juce::Slider swingSlider;
    juce::Label errorLabel;

    // Created after the combo boxes are populated so the initial selection shows.
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> enabledAttachment;
    std::unique_ptr<ComboBoxAttachment> modeAttachment, rateAttachment;
    std::unique_ptr<SliderAttachment> numStepsAttachment, gateAttachment, octaveRangeAttachment, swingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpExcelAudioProcessorEditor)
};
