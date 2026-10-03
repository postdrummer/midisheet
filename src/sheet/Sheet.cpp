#include "Sheet.h"

#include <algorithm>

namespace arp {

Sheet::Sheet()
{
    setupDefaultSheet();
}

// ---------------------------------------------------------------------------
// Column management

int Sheet::addColumn(ColumnType type, const std::string& name)
{
    Column c;
    c.type = type;
    c.name = !name.empty() ? name : columnTypeName(type);
    c.defaultValue = type == ColumnType::Velocity ? 100.0
                        : type == ColumnType::Percent || type == ColumnType::Chance ? 100.0
                        : 0.0;
    columns_.push_back(c);
    values_.emplace_back(static_cast<size_t>(kMaxRows), 0.0);
    hasValues_.emplace_back(static_cast<size_t>(kMaxRows), false);
    formulas_.emplace_back(static_cast<size_t>(kMaxRows));
    return getNumColumns() - 1;
}

void Sheet::removeColumn(int index)
{
    if (index < 0 || index >= getNumColumns())
        return;
    columns_.erase(columns_.begin() + index);
    values_.erase(values_.begin() + index);
    hasValues_.erase(hasValues_.begin() + index);
    formulas_.erase(formulas_.begin() + index);
}

void Sheet::moveColumn(int from, int to)
{
    if (from < 0 || from >= getNumColumns() || to < 0 || to >= getNumColumns() || from == to)
        return;
    auto move = [](auto& v, int f, int t) {
        auto it = v.begin() + f;
        auto val = std::move(*it);
        v.erase(it);
        v.insert(v.begin() + t, std::move(val));
    };
    move(columns_, from, to);
    move(values_, from, to);
    move(hasValues_, from, to);
    move(formulas_, from, to);
}

void Sheet::setColumnName(int col, const std::string& name)
{
    if (col >= 0 && col < getNumColumns() && !name.empty())
        columns_[static_cast<size_t>(col)].name = name;
}

void Sheet::setColumnType(int col, ColumnType type)
{
    if (col >= 0 && col < getNumColumns())
        columns_[static_cast<size_t>(col)].type = type;
}

void Sheet::setColumnVisible(int col, bool visible)
{
    if (col >= 0 && col < getNumColumns())
        columns_[static_cast<size_t>(col)].visible = visible;
}

void Sheet::setColumnDefault(int col, double value)
{
    if (col >= 0 && col < getNumColumns())
        columns_[static_cast<size_t>(col)].defaultValue = value;
}

void Sheet::setColumnDefaultFormula(int col, const std::string& formula)
{
    if (col >= 0 && col < getNumColumns())
        columns_[static_cast<size_t>(col)].defaultFormula = formula;
}

void Sheet::setColumnCCNumber(int col, int cc)
{
    if (col >= 0 && col < getNumColumns())
        columns_[static_cast<size_t>(col)].ccNumber = cc;
}

Column& Sheet::getColumn(int index)
{
    static Column fallback;
    if (index < 0 || index >= getNumColumns())
        return fallback;
    return columns_[static_cast<size_t>(index)];
}

const Column& Sheet::getColumn(int index) const
{
    static const Column fallback;
    if (index < 0 || index >= getNumColumns())
        return fallback;
    return columns_[static_cast<size_t>(index)];
}

int Sheet::findColumnByType(ColumnType type, bool visibleOnly) const
{
    for (int i = 0; i < getNumColumns(); ++i) {
        const auto& col = columns_[static_cast<size_t>(i)];
        if (col.type == type && (!visibleOnly || col.visible))
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Cell access

void Sheet::setCell(int col, int row, double value)
{
    if (col < 0 || col >= getNumColumns() || row < 0 || row >= kMaxRows)
        return;
    values_[static_cast<size_t>(col)][static_cast<size_t>(row)] = value;
    hasValues_[static_cast<size_t>(col)][static_cast<size_t>(row)] = true;
    formulas_[static_cast<size_t>(col)][static_cast<size_t>(row)].clear();
}

void Sheet::setCellFormula(int col, int row, const std::string& formula)
{
    if (col < 0 || col >= getNumColumns() || row < 0 || row >= kMaxRows)
        return;
    formulas_[static_cast<size_t>(col)][static_cast<size_t>(row)] = formula;
    hasValues_[static_cast<size_t>(col)][static_cast<size_t>(row)] = false;
}

void Sheet::clearCell(int col, int row)
{
    if (col < 0 || col >= getNumColumns() || row < 0 || row >= kMaxRows)
        return;
    values_[static_cast<size_t>(col)][static_cast<size_t>(row)] = 0.0;
    hasValues_[static_cast<size_t>(col)][static_cast<size_t>(row)] = false;
    formulas_[static_cast<size_t>(col)][static_cast<size_t>(row)].clear();
}

double Sheet::getCellValue(int col, int row) const
{
    if (col < 0 || col >= getNumColumns() || row < 0 || row >= kMaxRows)
        return 0.0;
    return values_[static_cast<size_t>(col)][static_cast<size_t>(row)];
}

const std::string& Sheet::getCellFormula(int col, int row) const
{
    static const std::string empty;
    if (col < 0 || col >= getNumColumns() || row < 0 || row >= kMaxRows)
        return empty;
    return formulas_[static_cast<size_t>(col)][static_cast<size_t>(row)];
}

bool Sheet::hasCellValue(int col, int row) const
{
    if (col < 0 || col >= getNumColumns() || row < 0 || row >= kMaxRows)
        return false;
    return hasValues_[static_cast<size_t>(col)][static_cast<size_t>(row)];
}

// ---------------------------------------------------------------------------
// Step active flags

void Sheet::setStepActive(int row, bool active)
{
    if (row >= 0 && row < kMaxRows)
        stepActive_[static_cast<size_t>(row)] = active;
}

bool Sheet::isStepActive(int row) const
{
    if (row < 0 || row >= kMaxRows)
        return false;
    return stepActive_[static_cast<size_t>(row)];
}

// ---------------------------------------------------------------------------
// Compile

std::unique_ptr<CompiledSheet> Sheet::compile(juce::StringArray& errors) const
{
    auto out = std::make_unique<CompiledSheet>();
    out->numCols = getNumColumns();
    out->numRows = kMaxRows;

    for (int c = 0; c < out->numCols; ++c) {
        const auto& src = columns_[static_cast<size_t>(c)];
        auto& dst = out->cols[static_cast<size_t>(c)];
        dst.type = src.type;
        dst.visible = src.visible;
        dst.defaultValue = src.defaultValue;
        dst.ccNumber = src.ccNumber;

        // Column default formula.
        if (!src.defaultFormula.empty()) {
            std::string err;
            out->columnDefaultPrograms[static_cast<size_t>(c)] =
                std::make_unique<formula::Program>(
                    formula::Program::compile(src.defaultFormula, err, out->numCols));
            if (!err.empty())
                errors.add(columnLetters(c) + ": " + err);
        }

        // Per-cell data.
        for (int r = 0; r < kMaxRows; ++r) {
            const int idx = c * kMaxRows + r;
            out->hasValue[static_cast<size_t>(idx)] = hasValues_[static_cast<size_t>(c)][static_cast<size_t>(r)];
            out->cells[static_cast<size_t>(idx)] = values_[static_cast<size_t>(c)][static_cast<size_t>(r)];

            const auto& f = formulas_[static_cast<size_t>(c)][static_cast<size_t>(r)];
            if (!f.empty()) {
                std::string err;
                out->cellPrograms[static_cast<size_t>(idx)] = std::make_unique<formula::Program>(
                    formula::Program::compile(f, err, out->numCols));
                if (!err.empty())
                    errors.add(columnLetters(c) + std::to_string(r + 1) + ": " + err);
            }
        }
    }

    for (int r = 0; r < kMaxRows; ++r)
        out->stepActive[static_cast<size_t>(r)] = stepActive_[static_cast<size_t>(r)];

    return out;
}

// ---------------------------------------------------------------------------
// Serialization

juce::var Sheet::toVar() const
{
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> cols;
    for (int c = 0; c < getNumColumns(); ++c) {
        const auto& col = columns_[static_cast<size_t>(c)];
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", juce::String(col.name));
        obj->setProperty("type", static_cast<int>(col.type));
        obj->setProperty("visible", col.visible);
        obj->setProperty("default", col.defaultValue);
        obj->setProperty("defaultFormula", juce::String(col.defaultFormula));
        obj->setProperty("ccNumber", col.ccNumber);
        cols.add(juce::var(obj));
    }
    root->setProperty("columns", cols);

    // Only serialize non-empty cells.
    auto* cells = new juce::DynamicObject();
    for (int c = 0; c < getNumColumns(); ++c) {
        for (int r = 0; r < kMaxRows; ++r) {
            const juce::String ref = juce::String(columnLetters(c)) + juce::String(r + 1);
            if (hasValues_[static_cast<size_t>(c)][static_cast<size_t>(r)])
                cells->setProperty(ref, values_[static_cast<size_t>(c)][static_cast<size_t>(r)]);
            else if (!formulas_[static_cast<size_t>(c)][static_cast<size_t>(r)].empty())
                cells->setProperty(ref, juce::String(formulas_[static_cast<size_t>(c)][static_cast<size_t>(r)]));
        }
    }
    root->setProperty("cells", juce::var(cells));

    juce::Array<juce::var> active;
    for (int r = 0; r < kMaxRows; ++r)
        if (stepActive_[static_cast<size_t>(r)])
            active.add(r + 1);
    root->setProperty("activeSteps", active);

    return juce::var(root);
}

void Sheet::fromVar(const juce::var& v)
{
    columns_.clear();
    values_.clear();
    hasValues_.clear();
    formulas_.clear();
    stepActive_.assign(static_cast<size_t>(kMaxRows), false);

    if (auto* cols = v["columns"].getArray()) {
        for (int i = 0; i < cols->size() && i < kMaxColumns; ++i) {
            const auto& obj = cols->getReference(i);
            Column c;
            c.name = obj["name"].toString().toStdString();
            c.type = static_cast<ColumnType>(static_cast<int>(obj["type"]));
            c.visible = obj.hasProperty("visible") ? static_cast<bool>(obj["visible"]) : true;
            c.defaultValue = obj.hasProperty("default") ? static_cast<double>(obj["default"]) : 0.0;
            c.defaultFormula = obj["defaultFormula"].toString().toStdString();
            c.ccNumber = obj.hasProperty("ccNumber") ? static_cast<int>(obj["ccNumber"]) : 0;
            columns_.push_back(c);
            values_.emplace_back(static_cast<size_t>(kMaxRows), 0.0);
            hasValues_.emplace_back(static_cast<size_t>(kMaxRows), false);
            formulas_.emplace_back(static_cast<size_t>(kMaxRows));
        }
    }

    if (auto* cells = v["cells"].getDynamicObject()) {
        for (const auto& prop : cells->getProperties()) {
            const auto ref = prop.name.toString().trim().toUpperCase();
            // "I12" -> letters "I", row 12. columnIndex() only accepts letters,
            // so split the trailing row number off first.
            int split = 0;
            while (split < ref.length() && ref[split] >= 'A' && ref[split] <= 'Z')
                ++split;
            const int c = columnIndex(ref.substring(0, split).toStdString());
            if (c < 0 || c >= getNumColumns())
                continue;
            const int row = ref.substring(split).getIntValue() - 1;
            if (row < 0 || row >= kMaxRows)
                continue;
            const auto& val = prop.value;
            if (val.isString()) {
                formulas_[static_cast<size_t>(c)][static_cast<size_t>(row)] = val.toString().toStdString();
                hasValues_[static_cast<size_t>(c)][static_cast<size_t>(row)] = false;
            } else {
                values_[static_cast<size_t>(c)][static_cast<size_t>(row)] = static_cast<double>(val);
                hasValues_[static_cast<size_t>(c)][static_cast<size_t>(row)] = true;
            }
        }
    }

    if (auto* active = v["activeSteps"].getArray())
        for (int i = 0; i < active->size(); ++i) {
            int r = static_cast<int>(active->getReference(i)) - 1;
            if (r >= 0 && r < kMaxRows)
                stepActive_[static_cast<size_t>(r)] = true;
        }

    if (columns_.empty())
        setupDefaultSheet();
}

void Sheet::clear()
{
    columns_.clear();
    values_.clear();
    hasValues_.clear();
    formulas_.clear();
    stepActive_.assign(static_cast<size_t>(kMaxRows), false);
    setupDefaultSheet();
}

// ---------------------------------------------------------------------------
// Default sheet

void Sheet::setupDefaultSheet()
{
    // Columns matching the vision: Note, Shift, Octave, Velocity, Gate, Length,
    // Time, Chance. Note/Time/Chance start hidden (pass-through / inactive).

    const auto add = [this](ColumnType type, const std::string& name, bool visible,
                            double def, const std::string& defFormula = {}) {
        int c = addColumn(type, name);
        setColumnVisible(c, visible);
        setColumnDefault(c, def);
        if (!defFormula.empty())
            setColumnDefaultFormula(c, defFormula);
        return c;
    };

    add(ColumnType::Note, "Note", false, -1.0); // hidden: pass through incoming note
    add(ColumnType::Shift, "Shift", true, 0.0);
    add(ColumnType::Octave, "Octave", true, 0.0);
    add(ColumnType::Velocity, "Velocity", true, 100.0, "=IF(MOD(STEP,4)=0,127,80)");
    add(ColumnType::Gate, "Gate", true, 50.0);
    add(ColumnType::Length, "Length", true, 1.0, "=IF(MOD(STEP,8)=7,2,1)");
    add(ColumnType::Time, "Time", false, 0.0);
    add(ColumnType::Chance, "Chance", false, 100.0);

    // All steps active by default.
    stepActive_.assign(static_cast<size_t>(kMaxRows), true);
}

} // namespace arp
