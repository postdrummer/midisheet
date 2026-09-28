#include "ArpPattern.h"

namespace arp {

ArpPattern::ArpPattern() {
    steps_.resize(MAX_STEPS);
}

ArpStep& ArpPattern::getStep(int index) {
    return steps_[juce::jlimit(0, MAX_STEPS - 1, index)];
}

const ArpStep& ArpPattern::getStep(int index) const {
    return steps_[juce::jlimit(0, MAX_STEPS - 1, index)];
}

void ArpPattern::setStep(int index, const ArpStep& step) {
    if (index >= 0 && index < MAX_STEPS) {
        steps_[index] = step;
    }
}

void ArpPattern::setNoteFormula(int step, const juce::String& formula) {
    if (step >= 0 && step < MAX_STEPS) steps_[step].noteFormula = formula;
}

void ArpPattern::setVelocityFormula(int step, const juce::String& formula) {
    if (step >= 0 && step < MAX_STEPS) steps_[step].velocityFormula = formula;
}

void ArpPattern::setGateFormula(int step, const juce::String& formula) {
    if (step >= 0 && step < MAX_STEPS) steps_[step].gateFormula = formula;
}

void ArpPattern::setLengthFormula(int step, const juce::String& formula) {
    if (step >= 0 && step < MAX_STEPS) steps_[step].lengthFormula = formula;
}

juce::String ArpPattern::getNoteFormula(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].noteFormula;
    return {};
}

juce::String ArpPattern::getVelocityFormula(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].velocityFormula;
    return {};
}

juce::String ArpPattern::getGateFormula(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].gateFormula;
    return {};
}

juce::String ArpPattern::getLengthFormula(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].lengthFormula;
    return {};
}

void ArpPattern::evaluate(FormulaEngine& engine, int currentStep, int inputNote, int inputVelocity) {
    engine.setVariable("STEP", currentStep);
    engine.setVariable("NOTE", inputNote);
    engine.setVariable("VELOCITY", inputVelocity);

    for (int i = 0; i < numSteps; i++) {
        ArpStep& step = steps_[i];

        // Evaluate note formula
        if (step.noteFormula.isNotEmpty()) {
            double val = engine.evaluate(step.noteFormula);
            step.note = juce::jlimit(0, 127, static_cast<int>(val));
        }

        // Evaluate velocity formula
        if (step.velocityFormula.isNotEmpty()) {
            double val = engine.evaluate(step.velocityFormula);
            step.velocity = juce::jlimit(1, 127, static_cast<int>(val));
        }

        // Evaluate gate formula
        if (step.gateFormula.isNotEmpty()) {
            double val = engine.evaluate(step.gateFormula);
            step.gate = juce::jlimit(0.0, 1.0, val / 100.0);
        }

        // Evaluate length formula
        if (step.lengthFormula.isNotEmpty()) {
            double val = engine.evaluate(step.lengthFormula);
            step.length = juce::jlimit(0.1, 16.0, val);
        }
    }
}

void ArpPattern::clear() {
    for (auto& step : steps_) {
        step = ArpStep();
    }
    numSteps = 16;
}

juce::var ArpPattern::toVar() const {
    juce::Array<juce::var> arr;
    for (int i = 0; i < numSteps; i++) {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("noteFormula", steps_[i].noteFormula);
        obj->setProperty("velocityFormula", steps_[i].velocityFormula);
        obj->setProperty("gateFormula", steps_[i].gateFormula);
        obj->setProperty("lengthFormula", steps_[i].lengthFormula);
        obj->setProperty("active", steps_[i].active);
        arr.add(obj.get());
    }
    return arr;
}

void ArpPattern::fromVar(const juce::var& v) {
    if (v.isArray()) {
        auto* arr = v.getArray();
        numSteps = juce::jlimit(1, MAX_STEPS, static_cast<int>(arr->size()));
        for (int i = 0; i < numSteps && i < arr->size(); i++) {
            auto* obj = arr->getObjectPointer(i);
            if (obj) {
                steps_[i].noteFormula = obj->getProperty("noteFormula");
                steps_[i].velocityFormula = obj->getProperty("velocityFormula");
                steps_[i].gateFormula = obj->getProperty("gateFormula");
                steps_[i].lengthFormula = obj->getProperty("lengthFormula");
                steps_[i].active = obj->getProperty("active");
            }
        }
    }
}

int ArpPattern::getNote(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].note;
    return 60;
}

int ArpPattern::getVelocity(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].velocity;
    return 100;
}

double ArpPattern::getGate(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].gate;
    return 0.5;
}

double ArpPattern::getLength(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].length;
    return 1.0;
}

bool ArpPattern::isStepActive(int step) const {
    if (step >= 0 && step < MAX_STEPS) return steps_[step].active;
    return false;
}

void ArpPattern::setStepActive(int step, bool active) {
    if (step >= 0 && step < MAX_STEPS) steps_[step].active = active;
}

} // namespace arp
