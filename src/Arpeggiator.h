#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "ArpPattern.h"
#include "FormulaEngine.h"

namespace arp {

enum class ArpMode {
    Up,
    Down,
    UpDown,
    DownUp,
    Random,
    Order,
    Chord
};

enum class ArpRate {
    Rate1_1 = 0,
    Rate1_2,
    Rate1_4,
    Rate1_8,
    Rate1_16,
    Rate1_32,
    Rate1_64
};

class Arpeggiator {
public:
    Arpeggiator();
    ~Arpeggiator() = default;

    void process(juce::MidiBuffer& midiMessages, int numSamples, double bpm);

    ArpPattern& getPattern() { return pattern_; }
    const ArpPattern& getPattern() const { return pattern_; }
    FormulaEngine& getFormulaEngine() { return engine_; }

    void setMode(ArpMode mode) { mode_ = mode; }
    ArpMode getMode() const { return mode_; }

    void setRate(ArpRate rate) { rate_ = rate; }
    ArpRate getRate() const { return rate_; }

    void setGatePercent(double gate) { gatePercent_ = juce::jlimit(0.0, 1.0, gate); }
    double getGatePercent() const { return gatePercent_; }

    void setOctaveRange(int range) { octaveRange_ = juce::jlimit(1, 8, range); }
    int getOctaveRange() const { return octaveRange_; }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    void setSwing(double swing) { swing_ = juce::jlimit(0.0, 1.0, swing); }
    double getSwing() const { return swing_; }

    int getCurrentStep() const { return currentStep_; }
    int getCurrentNote() const { return currentNote_; }
    int getCurrentVelocity() const { return currentVelocity_; }
    double getCurrentGate() const { return currentGate_; }
    double getCurrentLength() const { return currentLength_; }

    void reset();
    void setInputNote(int note, int velocity);
    void setChordNotes(const std::vector<int>& notes);
    const std::vector<int>& getChordNotes() const { return chordNotes_; }

    double getRateFraction() const;
    int getNumSteps() const { return pattern_.getNumSteps(); }
    void setNumSteps(int steps) { pattern_.setNumSteps(steps); }

    // Get the current step's computed values
    int getCurrentStepNote() const { return currentStepNote_; }
    int getCurrentStepVelocity() const { return currentStepVelocity_; }
    double getCurrentStepGate() const { return currentStepGate_; }
    double getCurrentStepLength() const { return currentStepLength_; }
    bool isCurrentStepActive() const { return currentStepActive_; }

private:
    ArpPattern pattern_;
    FormulaEngine engine_;

    ArpMode mode_ = ArpMode::Up;
    ArpRate rate_ = ArpRate::Rate1_16;
    double gatePercent_ = 0.5;
    int octaveRange_ = 1;
    bool enabled_ = true;
    double swing_ = 0.0;

    int currentStep_ = 0;
    int currentNote_ = 60;
    int currentVelocity_ = 100;
    double currentGate_ = 0.5;
    double currentLength_ = 1.0;

    int currentStepNote_ = 60;
    int currentStepVelocity_ = 100;
    double currentStepGate_ = 0.5;
    double currentStepLength_ = 1.0;
    bool currentStepActive_ = true;

    std::vector<int> chordNotes_;
    std::vector<int> orderedNotes_;

    double sampleRate_ = 44100.0;
    double bpm_ = 120.0;
    double samplesPerStep_ = 0.0;
    double sampleCounter_ = 0.0;

    void updateOrderedNotes();
    void advanceStep();
};

} // namespace arp
