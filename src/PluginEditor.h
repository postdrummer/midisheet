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

    juce::AudioProcessorValueTreeState::ButtonAttachment enabledAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment modeAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment rateAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment numStepsAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment gateAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment octaveRangeAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment swingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpExcelAudioProcessorEditor)
};
