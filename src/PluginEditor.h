#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "TrackerGrid.h"

class MidisheetAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit MidisheetAudioProcessorEditor(MidisheetAudioProcessor&);
    ~MidisheetAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    void showSelectedCell();
    void commitFormula();

    MidisheetAudioProcessor& processorRef;

    juce::ToggleButton enabledButton;
    juce::ComboBox modeCombo;
    juce::ComboBox rateCombo;
    juce::Slider numStepsSlider;
    juce::Slider gateSlider;
    juce::Slider octaveRangeSlider;
    juce::Slider swingSlider;
    juce::Label numStepsLabel, gateLabel, octaveRangeLabel, swingLabel;

    juce::Label cellLabel;       // e.g. "5 vel"
    juce::TextEditor formulaBar; // formula of the selected cell
    juce::Label statusLabel;     // compile error or key hints
    TrackerGrid grid;

    // Created after the combo boxes are populated so the initial selection shows.
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> enabledAttachment;
    std::unique_ptr<ComboBoxAttachment> modeAttachment, rateAttachment;
    std::unique_ptr<SliderAttachment> numStepsAttachment, gateAttachment, octaveRangeAttachment, swingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidisheetAudioProcessorEditor)
};
