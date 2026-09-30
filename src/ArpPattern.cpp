#include "ArpPattern.h"

namespace arp {

ArpPattern::ArpPattern()
{
    clear();
}

ArpStep& ArpPattern::getStep(int index)
{
    return steps_[static_cast<size_t>(juce::jlimit(0, MAX_STEPS - 1, index))];
}

const ArpStep& ArpPattern::getStep(int index) const
{
    return steps_[static_cast<size_t>(juce::jlimit(0, MAX_STEPS - 1, index))];
}

bool ArpPattern::cellIndex(const juce::String& ref, int& index)
{
    auto r = ref.trim().toUpperCase();
    if (r.length() < 2 || r[0] < 'A' || r[0] >= 'A' + formula::kCellCols
        || !r.substring(1).containsOnly("0123456789"))
        return false;
    int row = r.substring(1).getIntValue();
    if (row < 1 || row > formula::kCellRows)
        return false;
    index = (r[0] - 'A') * formula::kCellRows + (row - 1);
    return true;
}

void ArpPattern::setCell(const juce::String& ref, double value)
{
    int i;
    if (cellIndex(ref, i))
        cells_[static_cast<size_t>(i)] = value;
}

double ArpPattern::getCell(const juce::String& ref) const
{
    int i;
    return cellIndex(ref, i) ? cells_[static_cast<size_t>(i)] : 0.0;
}

std::unique_ptr<Pattern> ArpPattern::compile(juce::StringArray& errors) const
{
    auto out = std::make_unique<Pattern>();
    out->cells = cells_;

    auto compileOne = [&](const juce::String& text, formula::Program& prog, int step, const char* lane) {
        if (text.trim().isEmpty())
            return;
        std::string err;
        prog = formula::Program::compile(text.toStdString(), err);
        if (!err.empty())
            errors.add(juce::String(step + 1) + " " + lane + ": " + juce::String(err));
    };

    for (int i = 0; i < MAX_STEPS; ++i) {
        const auto& src = steps_[static_cast<size_t>(i)];
        auto& dst = out->steps[static_cast<size_t>(i)];
        dst.active = src.active;
        compileOne(src.noteFormula, dst.note, i, "note");
        compileOne(src.velocityFormula, dst.velocity, i, "velocity");
        compileOne(src.gateFormula, dst.gate, i, "gate");
        compileOne(src.lengthFormula, dst.length, i, "length");
    }
    return out;
}

void ArpPattern::clear()
{
    for (auto& step : steps_)
        step = ArpStep();
    cells_.fill(0.0);

    // Demo pattern: accent every beat, tie the last step of each half-bar.
    // Note formulas are left empty so the pattern follows the held notes.
    for (auto& step : steps_) {
        step.velocityFormula = "=IF(MOD(STEP,4)=0,127,80)";
        step.lengthFormula = "=IF(MOD(STEP,8)=7,2,1)";
    }
}

juce::var ArpPattern::toVar() const
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> steps;
    for (const auto& s : steps_) {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("noteFormula", s.noteFormula);
        obj->setProperty("velocityFormula", s.velocityFormula);
        obj->setProperty("gateFormula", s.gateFormula);
        obj->setProperty("lengthFormula", s.lengthFormula);
        obj->setProperty("active", s.active);
        steps.add(juce::var(obj));
    }
    root->setProperty("steps", steps);

    auto* cells = new juce::DynamicObject();
    for (int i = 0; i < static_cast<int>(cells_.size()); ++i)
        if (cells_[static_cast<size_t>(i)] != 0.0)
            cells->setProperty(juce::String::charToString(static_cast<juce::juce_wchar>('A' + i / formula::kCellRows))
                                   + juce::String(i % formula::kCellRows + 1),
                               cells_[static_cast<size_t>(i)]);
    root->setProperty("cells", juce::var(cells));
    return juce::var(root);
}

void ArpPattern::fromVar(const juce::var& v)
{
    if (auto* steps = v["steps"].getArray()) {
        for (int i = 0; i < MAX_STEPS; ++i) {
            auto& s = steps_[static_cast<size_t>(i)];
            s = ArpStep();
            if (i >= steps->size())
                continue;
            const auto& obj = steps->getReference(i);
            s.noteFormula = obj["noteFormula"].toString();
            s.velocityFormula = obj["velocityFormula"].toString();
            s.gateFormula = obj["gateFormula"].toString();
            s.lengthFormula = obj["lengthFormula"].toString();
            s.active = obj.hasProperty("active") ? static_cast<bool>(obj["active"]) : true;
        }
    }
    cells_.fill(0.0);
    if (auto* cells = v["cells"].getDynamicObject())
        for (const auto& prop : cells->getProperties())
            setCell(prop.name.toString(), static_cast<double>(prop.value));
}

} // namespace arp
