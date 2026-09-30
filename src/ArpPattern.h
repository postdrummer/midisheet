#pragma once

#include <juce_core/juce_core.h>

#include "engine/ArpEngine.h"

#include <memory>

namespace arp {

/**
 * The editable pattern: formula text for each step, plus the cell grid that
 * formulas can reference (A1..H64). Lives on the message thread; compile()
 * turns it into an engine Pattern for the audio thread.
 */
struct ArpStep {
    juce::String noteFormula;     // e.g. "=NOTE+7"; empty = the arp note
    juce::String velocityFormula; // e.g. "=VELOCITY-10"; empty = the held velocity
    juce::String gateFormula;     // percent of the step, e.g. "=50"; empty = Gate knob
    juce::String lengthFormula;   // in steps, e.g. "=2"; empty = 1
    bool active = true;
};

class ArpPattern {
public:
    static constexpr int MAX_STEPS = kMaxSteps;

    ArpPattern();

    ArpStep& getStep(int index);
    const ArpStep& getStep(int index) const;

    void setCell(const juce::String& ref, double value); // e.g. "A1"
    double getCell(const juce::String& ref) const;

    // Compiles every formula. Errors are reported per step as "3 note: Unknown name: FOO".
    std::unique_ptr<Pattern> compile(juce::StringArray& errors) const;

    void clear();

    juce::var toVar() const;
    void fromVar(const juce::var& v);

    static bool cellIndex(const juce::String& ref, int& index);

private:
    std::array<ArpStep, MAX_STEPS> steps_;
    std::array<double, formula::kCellCols * formula::kCellRows> cells_{};
};

} // namespace arp
