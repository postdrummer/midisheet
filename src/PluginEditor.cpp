#include "PluginEditor.h"

ArpExcelAudioProcessorEditor::ArpExcelAudioProcessorEditor(ArpExcelAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      enabledAttachment(p.getAPVTS(), "enabled", enabledButton),
      modeAttachment(p.getAPVTS(), "mode", modeCombo),
      rateAttachment(p.getAPVTS(), "rate", rateCombo),
      numStepsAttachment(p.getAPVTS(), "numSteps", numStepsSlider),
      gateAttachment(p.getAPVTS(), "gate", gateSlider),
      octaveRangeAttachment(p.getAPVTS(), "octaveRange", octaveRangeSlider),
      swingAttachment(p.getAPVTS(), "swing", swingSlider)
{
    enabledButton.setButtonText("Enabled");
    addAndMakeVisible(enabledButton);

    modeCombo.addItemList({"Up", "Down", "Up-Down", "Down-Up", "Random", "Order", "Chord"}, 1);
    addAndMakeVisible(modeCombo);

    rateCombo.addItemList({"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"}, 1);
    addAndMakeVisible(rateCombo);

    numStepsSlider.setRange(1, 64, 1);
    numStepsSlider.setNumDecimalPlacesToDisplay(0);
    addAndMakeVisible(numStepsSlider);

    gateSlider.setRange(0.0, 1.0, 0.01);
    addAndMakeVisible(gateSlider);

    octaveRangeSlider.setRange(1, 8, 1);
    octaveRangeSlider.setNumDecimalPlacesToDisplay(0);
    addAndMakeVisible(octaveRangeSlider);

    swingSlider.setRange(0.0, 1.0, 0.01);
    addAndMakeVisible(swingSlider);

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
}
