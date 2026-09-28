#pragma once

#include <juce_core/juce_core.h>
#include "FormulaEngine.h"

namespace arp {

/**
 * Represents a single step in the arpeggiator pattern.
 * Each step can have a formula for note, velocity, and gate length.
 */
struct ArpStep {
    juce::String noteFormula;      // e.g., "=A1+12" or "=NOTE+7"
    juce::String velocityFormula;  // e.g., "=100" or "=VELOCITY-10"
    juce::String gateFormula;      // e.g., "=50" or "=LENGTH*0.5"
    juce::String lengthFormula;    // e.g., "=100" or "=LENGTH"

    bool active = true;
    int note = 60;                 // Cached computed value
    int velocity = 100;            // Cached computed value
    double gate = 0.5;             // Cached computed value (0-1)
    double length = 1.0;           // Cached computed value (in steps)

    ArpStep() = default;
};

/**
 * The arpeggiator pattern - a grid of steps with Excel-like formulas.
 * Supports up to 64 steps, each with note/velocity/gate/length formulas.
 */
class ArpPattern {
public:
    static constexpr int MAX_STEPS = 64;

    ArpPattern();

    // Get/set step
    ArpStep& getStep(int index);
    const ArpStep& getStep(int index) const;
    void setStep(int index, const ArpStep& step);

    // Set formulas for a step
    void setNoteFormula(int step, const juce::String& formula);
    void setVelocityFormula(int step, const juce::String& formula);
    void setGateFormula(int step, const juce::String& formula);
    void setLengthFormula(int step, const juce::String& formula);

    // Get formulas
    juce::String getNoteFormula(int step) const;
    juce::String getVelocityFormula(int step) const;
    juce::String getGateFormula(int step) const;
    juce::String getLengthFormula(int step) const;

    // Pattern properties
    int getNumSteps() const { return numSteps; }
    void setNumSteps(int steps) { numSteps = juce::jlimit(1, MAX_STEPS, steps); }

    // Evaluate all steps using the formula engine
    void evaluate(FormulaEngine& engine, int currentStep, int inputNote, int inputVelocity);

    // Clear all steps
    void clear();

    // Serialize/deserialize
    juce::var toVar() const;
    void fromVar(const juce::var& v);

    // Get the computed note for a step
    int getNote(int step) const;
    int getVelocity(int step) const;
    double getGate(int step) const;
    double getLength(int step) const;

    // Check if step is active
    bool isStepActive(int step) const;
    void setStepActive(int step, bool active);

private:
    std::vector<ArpStep> steps_;
    int numSteps = 16;
};

} // namespace arp
