#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "sheet/Sheet.h"
#include "engine/ArpEngine.h"
#include "engine/PatternExchange.h"

class MidisheetAudioProcessor : public juce::AudioProcessor {
public:
    MidisheetAudioProcessor();
    ~MidisheetAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Midisheet"; }
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

    // Sheet editing (message thread only). Call sheetChanged() after editing
    // to recompile and hand the new sheet to the audio thread.
    arp::Sheet& getSheet() { return sheet_; }
    juce::StringArray sheetChanged();
    const juce::StringArray& getSheetErrors() const { return sheetErrors_; }
    int getSheetVersion() const { return sheetVersion_.load(); } // bumps on every sheetChanged()
    int getPlayingStep() const { return playingStep_.load(); }   // -1 until the first step

    // Parameters
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // MIDI-tab front panel settings (see private members below)
    std::atomic<int> midiInputChannel { 0 };  // 0 = All
    std::atomic<int> midiMinNote { 0 };
    std::atomic<int> midiMaxNote { 127 };
    std::atomic<int> midiCurve { 0 };
    std::atomic<int> midiOutputChannel { 1 };
    std::atomic<int> midiRouting { 0 };        // 0 = replace, 1 = augment
    std::atomic<bool> midiThru { false };

private:
    static BusesProperties makeBuses();
    void syncSettings();

    arp::ArpEngine engine_;
    arp::Sheet sheet_;                    // message thread
    juce::StringArray sheetErrors_;       // message thread
    arp::Exchange<arp::CompiledSheet> exchange_; // message -> audio thread
    std::vector<arp::MidiOut> events_;    // audio thread scratch
    juce::MidiBuffer outBuffer_;          // audio thread scratch
    std::atomic<int> sheetVersion_{0};
    std::atomic<int> playingStep_{-1};
    juce::AudioProcessorValueTreeState apvts_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidisheetAudioProcessor)
};
