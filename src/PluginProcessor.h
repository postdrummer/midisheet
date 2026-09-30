#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ArpPattern.h"
#include "engine/ArpEngine.h"
#include "engine/PatternExchange.h"

class ArpExcelAudioProcessor : public juce::AudioProcessor {
public:
    ArpExcelAudioProcessor();
    ~ArpExcelAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
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

    // Pattern editing (message thread only). Call patternChanged() after
    // editing to recompile and hand the new pattern to the audio thread.
    arp::ArpPattern& getPattern() { return pattern_; }
    juce::StringArray patternChanged();
    const juce::StringArray& getPatternErrors() const { return patternErrors_; }

    // Parameters
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    static BusesProperties makeBuses();
    void syncSettings();

    arp::ArpEngine engine_;
    arp::ArpPattern pattern_;          // message thread
    juce::StringArray patternErrors_;  // message thread
    arp::PatternExchange exchange_;    // message -> audio thread
    std::vector<arp::MidiOut> events_; // audio thread scratch
    juce::MidiBuffer outBuffer_;       // audio thread scratch
    juce::AudioProcessorValueTreeState apvts_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpExcelAudioProcessor)
};
