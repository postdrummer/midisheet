#pragma once

// The editable spreadsheet (message thread only).
//
// Columns are parameters (Note, Shift, Octave, Velocity, Gate, Length, Time,
// Chance, CC, ...); rows are steps in the arpeggiator sequence. Each cell
// holds a literal value or a formula referencing other cells. The sheet
// compiles to an immutable CompiledSheet for the audio thread.
//
// All editing methods are message-thread only. Call compile() (via
// PluginProcessor::patternChanged()) after editing to recompile and hand the
// new snapshot to the audio thread.

#include "Column.h"
#include "CompiledSheet.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <string>
#include <vector>

namespace arp {

class Sheet {
public:
    static constexpr int kMaxRows = arp::kMaxRows;

    Sheet();

    // --- Column management -------------------------------------------------

    // Adds a column of the given type. Returns the new column's index.
    int addColumn(ColumnType type, const std::string& name = {});
    void removeColumn(int index);
    void moveColumn(int from, int to); // reorder (drag or move left/right)

    void setColumnName(int col, const std::string& name);
    void setColumnType(int col, ColumnType type);
    void setColumnVisible(int col, bool visible);
    void setColumnDefault(int col, double value);
    void setColumnDefaultFormula(int col, const std::string& formula);
    void setColumnCCNumber(int col, int cc);

    int getNumColumns() const { return static_cast<int>(columns_.size()); }
    int getNumRows() const { return kMaxRows; }

    Column& getColumn(int index);
    const Column& getColumn(int index) const;
    // First column of the given type, or -1. By default only visible columns
    // match, so a hidden column is "not found"; pass visibleOnly = false to
    // include hidden columns (e.g. to find one in order to show it again).
    int findColumnByType(ColumnType type, bool visibleOnly = true) const;

    // --- Cell access -------------------------------------------------------

    void setCell(int col, int row, double value);
    void setCellFormula(int col, int row, const std::string& formula);
    void clearCell(int col, int row);

    double getCellValue(int col, int row) const;
    const std::string& getCellFormula(int col, int row) const;
    bool hasCellValue(int col, int row) const;

    // --- Step active flags -------------------------------------------------

    void setStepActive(int row, bool active);
    bool isStepActive(int row) const;

    // --- Compile -----------------------------------------------------------

    // Compiles every cell formula and column default formula. Errors are
    // reported as "ColRow: message" (e.g. "C3: Unknown name: FOO").
    std::unique_ptr<CompiledSheet> compile(juce::StringArray& errors) const;

    // --- Serialization -----------------------------------------------------

    juce::var toVar() const;
    void fromVar(const juce::var& v);

    void clear();

private:
    std::vector<Column> columns_;
    // Cell storage, column-major: [col][row].
    std::vector<std::vector<double>> values_;
    std::vector<std::vector<bool>> hasValues_;
    std::vector<std::vector<std::string>> formulas_;
    std::vector<bool> stepActive_;

    void setupDefaultSheet();
};

} // namespace arp
