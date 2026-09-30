#include "PluginEditor.h"

ArpExcelAudioProcessorEditor::ArpExcelAudioProcessorEditor(ArpExcelAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    auto& apvts = p.getAPVTS();

    enabledButton.setButtonText("Enabled");
    addAndMakeVisible(enabledButton);

    modeCombo.addItemList(apvts.getParameter("mode")->getAllValueStrings(), 1);
    addAndMakeVisible(modeCombo);

    rateCombo.addItemList(apvts.getParameter("rate")->getAllValueStrings(), 1);
    addAndMakeVisible(rateCombo);

    for (auto* s : {&numStepsSlider, &gateSlider, &octaveRangeSlider, &swingSlider})
        addAndMakeVisible(*s);

    enabledAttachment = std::make_unique<ButtonAttachment>(apvts, "enabled", enabledButton);
    modeAttachment = std::make_unique<ComboBoxAttachment>(apvts, "mode", modeCombo);
    rateAttachment = std::make_unique<ComboBoxAttachment>(apvts, "rate", rateCombo);
    numStepsAttachment = std::make_unique<SliderAttachment>(apvts, "numSteps", numStepsSlider);
    gateAttachment = std::make_unique<SliderAttachment>(apvts, "gate", gateSlider);
    octaveRangeAttachment = std::make_unique<SliderAttachment>(apvts, "octaveRange", octaveRangeSlider);
    swingAttachment = std::make_unique<SliderAttachment>(apvts, "swing", swingSlider);

    // Surface formula compile errors (unknown names, bad syntax) instead of silently playing 0.
    const auto& errors = p.getPatternErrors();
    errorLabel.setText(errors.isEmpty() ? juce::String() : "Formula errors: " + errors.joinIntoString("; "),
                       juce::dontSendNotification);
    errorLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(errorLabel);

    setSize(600, 400);
}

ArpExcelAudioProcessorEditor::~ArpExcelAudioProcessorEditor() {}

void ArpExcelAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::darkgrey.darker());
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("ArpExcel", 20, 20, 200, 30, juce::Justification::centredLeft);
}

void ArpExcelAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(40); // title
    enabledButton.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    modeCombo.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    rateCombo.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    numStepsSlider.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    gateSlider.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    octaveRangeSlider.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    swingSlider.setBounds(area.removeFromTop(30));
    errorLabel.setBounds(area);
}
