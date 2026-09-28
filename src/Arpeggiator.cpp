#include "Arpeggiator.h"
#include <algorithm>
#include <random>

namespace arp {

Arpeggiator::Arpeggiator() {
    pattern_.setNumSteps(16);

    // Default pattern: C major scale with Excel-like formulas
    int scale[] = {60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84, 86};
    for (int i = 0; i < 16; i++) {
        juce::String cell = FormulaEngine::rowColToCell(i, 0);
        engine_.setCell(cell, scale[i]);
        pattern_.setNoteFormula(i, "=" + cell + "+12");
        pattern_.setVelocityFormula(i, "=IF(MOD(STEP,4)=0,127,80)");
        pattern_.setGateFormula(i, "=50+MOD(STEP,4)*10");
        pattern_.setLengthFormula(i, "=IF(MOD(STEP,8)=7,2,1)");
    }
}

void Arpeggiator::process(juce::MidiBuffer& midiMessages, int numSamples, double bpm) {
    if (!enabled_) return;

    bpm_ = bpm;
    double secondsPerBeat = 60.0 / bpm;
    double secondsPerStep = secondsPerBeat * getRateFraction();
    samplesPerStep_ = secondsPerStep * sampleRate_;

    juce::MidiBuffer outputBuffer;

    // Pass through non-note messages and track chord notes
    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        int samplePos = metadata.samplePosition;

        if (msg.isNoteOn()) {
            setInputNote(msg.getNoteNumber(), msg.getVelocity());
            chordNotes_.push_back(msg.getNoteNumber());
            updateOrderedNotes();
        } else if (msg.isNoteOff()) {
            auto it = std::find(chordNotes_.begin(), chordNotes_.end(), msg.getNoteNumber());
            if (it != chordNotes_.end()) {
                chordNotes_.erase(it);
            }
            updateOrderedNotes();
        } else if (msg.isAllNotesOff()) {
            chordNotes_.clear();
            orderedNotes_.clear();
        }

        // Pass through all original messages
        outputBuffer.addEvent(msg, samplePos);
    }

    // Generate arpeggiated notes
    if (!chordNotes_.empty() && !orderedNotes_.empty()) {
        sampleCounter_ += numSamples;

        while (sampleCounter_ >= samplesPerStep_) {
            sampleCounter_ -= samplesPerStep_;
            advanceStep();

            if (currentStepActive_) {
                // Note on
                outputBuffer.addEvent(
                    juce::MidiMessage::noteOn(1, currentStepNote_, static_cast<juce::uint8>(currentStepVelocity_)),
                    0
                );

                // Note off (after gate duration)
                int gateSamples = static_cast<int>(samplesPerStep_ * currentStepGate_);
                outputBuffer.addEvent(
                    juce::MidiMessage::noteOff(1, currentStepNote_),
                    gateSamples
                );
            }
        }
    }

    midiMessages.swapWith(outputBuffer);
}

void Arpeggiator::updateOrderedNotes() {
    orderedNotes_ = chordNotes_;

    switch (mode_) {
        case ArpMode::Up:
            std::sort(orderedNotes_.begin(), orderedNotes_.end());
            break;
        case ArpMode::Down:
            std::sort(orderedNotes_.begin(), orderedNotes_.end(), std::greater<int>());
            break;
        case ArpMode::UpDown: {
            std::sort(orderedNotes_.begin(), orderedNotes_.end());
            if (orderedNotes_.size() > 2) {
                for (int i = static_cast<int>(orderedNotes_.size()) - 2; i > 0; i--) {
                    orderedNotes_.push_back(orderedNotes_[i]);
                }
            }
            break;
        }
        case ArpMode::DownUp: {
            std::sort(orderedNotes_.begin(), orderedNotes_.end(), std::greater<int>());
            if (orderedNotes_.size() > 2) {
                for (int i = static_cast<int>(orderedNotes_.size()) - 2; i > 0; i--) {
                    orderedNotes_.push_back(orderedNotes_[i]);
                }
            }
            break;
        }
        case ArpMode::Random: {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(orderedNotes_.begin(), orderedNotes_.end(), g);
            break;
        }
        case ArpMode::Order:
        case ArpMode::Chord:
            break;
    }

    // Apply octave range
    if (octaveRange_ > 1) {
        std::vector<int> original = orderedNotes_;
        for (int oct = 1; oct < octaveRange_; oct++) {
            for (int note : original) {
                orderedNotes_.push_back(note + (oct * 12));
            }
        }
    }
}

void Arpeggiator::advanceStep() {
    if (orderedNotes_.empty()) return;

    int stepIdx = currentStep_ % pattern_.getNumSteps();
    ArpStep& step = pattern_.getStep(stepIdx);

    // Evaluate formulas
    engine_.setVariable("STEP", currentStep_);
    engine_.setVariable("NOTE", orderedNotes_[0]);
    engine_.setVariable("VELOCITY", 100);

    int note = step.note;
    if (step.noteFormula.isNotEmpty()) {
        note = juce::jlimit(0, 127, static_cast<int>(engine_.evaluate(step.noteFormula)));
    } else {
        note = orderedNotes_[currentStep_ % orderedNotes_.size()];
    }

    int velocity = step.velocity;
    if (step.velocityFormula.isNotEmpty()) {
        velocity = juce::jlimit(1, 127, static_cast<int>(engine_.evaluate(step.velocityFormula)));
    }

    double gate = step.gate;
    if (step.gateFormula.isNotEmpty()) {
        gate = juce::jlimit(0.0, 1.0, engine_.evaluate(step.gateFormula) / 100.0);
    }

    double length = step.length;
    if (step.lengthFormula.isNotEmpty()) {
        length = juce::jlimit(0.1, 16.0, engine_.evaluate(step.lengthFormula));
    }

    currentStepNote_ = note;
    currentStepVelocity_ = velocity;
    currentStepGate_ = gate;
    currentStepLength_ = length;
    currentStepActive_ = step.active;

    currentStep_++;
}

void Arpeggiator::reset() {
    currentStep_ = 0;
    sampleCounter_ = 0.0;
    chordNotes_.clear();
    orderedNotes_.clear();
}

void Arpeggiator::setInputNote(int note, int velocity) {
    currentNote_ = note;
    currentVelocity_ = velocity;
}

void Arpeggiator::setChordNotes(const std::vector<int>& notes) {
    chordNotes_ = notes;
    updateOrderedNotes();
}

double Arpeggiator::getRateFraction() const {
    switch (rate_) {
        case ArpRate::Rate1_1: return 1.0;
        case ArpRate::Rate1_2: return 0.5;
        case ArpRate::Rate1_4: return 0.25;
        case ArpRate::Rate1_8: return 0.125;
        case ArpRate::Rate1_16: return 0.0625;
        case ArpRate::Rate1_32: return 0.03125;
        case ArpRate::Rate1_64: return 0.015625;
    }
    return 0.0625;
}

} // namespace arp
