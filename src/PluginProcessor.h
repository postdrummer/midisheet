#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Arpeggiator.h"

class ArpExcelAudioProcessor : public juce::AudioProcessor {
public:
    ArpExcelAudioProcessor();
    ~ArpExcelAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ArpExcel"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Access to the arpeggiator
    arp::Arpeggiator& getArpeggiator() { return arpeggiator_; }
    arp::ArpPattern& getPattern() { return arpeggiator_.getPattern(); }
    arp::FormulaEngine& getFormulaEngine() { return arpeggiator_.getFormulaEngine(); }

    // Parameters
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }

private:
    arp::Arpeggiator arpeggiator_;
    juce::AudioProcessorValueTreeState apvts_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpExcelAudioProcessor)
};
