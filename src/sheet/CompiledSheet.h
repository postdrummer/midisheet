#pragma once

// Compiled, audio-thread-safe snapshot of a Sheet.
//
// Immutable once published. The audio thread only reads: no allocation, no
// mutation, no locks. Formula programs are built at compile time (message
// thread) and stored sparsely via unique_ptr, so empty cells cost one null
// pointer. Evaluation order per cell:
//   cell formula  >  cell value  >  column default formula  >  column default
//
// Cell array is column-major (cells[col * numRows + row]) to match the
// formula engine's cell-reference indexing.
//
// A row is a step: evaluateCell() binds STEP to the row it evaluates, and
// cell references resolve through evaluateCell() too, so a referenced cell
// contributes its computed value (formula > value > column default) rather
// than its raw literal. Reference chains are depth-limited so circular
// references terminate on the audio thread.

#include "Column.h"
#include "../engine/Formula.h"

#include <array>
#include <memory>

namespace arp {

struct CompiledSheet {
    static constexpr int kMaxCols = kMaxColumns;
    static constexpr int kMaxRows = arp::kMaxRows;

    int numCols = 0;
    int numRows = kMaxRows;

    // How many rows the engine plays / the UI calls rows.
    int numSteps = kMaxRows;

    // Cell references index as col * kCellRows + row; the stride must match.
    static_assert(kMaxRows == formula::kCellRows, "cell reference stride mismatch");

    // Longest chain of cell references followed from one evaluation
    // (A1 -> B1 -> C1 ...). A circular reference stops here instead of
    // recursing without bound on the audio thread's stack.
    static constexpr int kMaxRefDepth = 16;

    // Column metadata (fixed size).
    std::array<ColumnMeta, kMaxCols> cols{};

    // Literal cell values, column-major. Only meaningful where hasValue is set.
    std::array<double, kMaxCols * kMaxRows> cells{};
    std::array<bool, kMaxCols * kMaxRows> hasValue{};

    // Per-step active flag (a step property, not a column).
    std::array<bool, kMaxRows> stepActive{};

    // Sparse compiled formulas: one per cell (null = no formula) and one per
    // column default (null = no default formula).
    std::array<std::unique_ptr<formula::Program>, kMaxCols * kMaxRows> cellPrograms{};
    std::array<std::unique_ptr<formula::Program>, kMaxCols> columnDefaultPrograms{};

    // Evaluate a cell: cell formula > cell value > column default formula >
    // column default. Real-time safe: no allocation, bounded recursion.
    double evaluateCell(int col, int row, const formula::Context& ctx) const
    {
        const size_t idx = static_cast<size_t>(col * numRows + row);
        if (ctx.depth >= kMaxRefDepth) // circular reference: stop the chain
            return hasValue[idx] ? cells[idx] : cols[static_cast<size_t>(col)].defaultValue;

        auto child = ctx;
        child.cells = cells.data();
        child.sheet = this;
        child.cellRef = &evalCellRef;
        child.depth = ctx.depth + 1;
        child.srcCol = col; // refs from this cell may not jump forward
        child.set(formula::Var::Step, row); // a cell lives in row `row`, i.e. step `row`

        if (cellPrograms[idx])
            return cellPrograms[idx]->eval(child);
        if (hasValue[idx])
            return cells[idx];
        if (columnDefaultPrograms[static_cast<size_t>(col)])
            return columnDefaultPrograms[static_cast<size_t>(col)]->eval(child);
        return cols[static_cast<size_t>(col)].defaultValue;
    }

    // Callback installed into formula::Context so cell references hop back
    // through evaluateCell(). `index` is column-major: col * kCellRows + row.
    //
    // A referenced cell contributes its computed value only when not to the
    // right of the source cell; otherwise we fall back to that column's
    // default chain (default formula > default value). Hidden columns count
    // as visible here: hiding is cosmetic and does not change evaluation.
    // This is the documented behavior for empty cells (handled in
    // evaluateCell itself) and forward references.
    static double evalCellRef(const void* self, int index, const formula::Context& ctx)
    {
        const auto& sheet = *static_cast<const CompiledSheet*>(self);
        const int col = index / sheet.numRows;
        const int row = index % sheet.numRows;
        if (col < 0 || col >= sheet.numCols)
            return 0.0;

        const auto& meta = sheet.cols[static_cast<size_t>(col)];
        const bool forward = ctx.srcCol >= 0 && col > ctx.srcCol;
        if (!forward)
            return sheet.evaluateCell(col, row, ctx);

        if (ctx.depth >= kMaxRefDepth)
            return meta.defaultValue;
        auto child = ctx;
        child.depth = ctx.depth + 1;
        child.srcCol = col;
        child.set(formula::Var::Step, row);
        if (sheet.columnDefaultPrograms[static_cast<size_t>(col)])
            return sheet.columnDefaultPrograms[static_cast<size_t>(col)]->eval(child);
        return meta.defaultValue;
    }

    // Find the first column of a given type (-1 if none). Hidden columns
    // are included: hiding is cosmetic and does not change evaluation.
    int findColumn(ColumnType type) const
    {
        for (int c = 0; c < numCols; ++c)
            if (cols[static_cast<size_t>(c)].type == type)
                return c;
        return -1;
    }

    bool isStepActive(int row) const { return stepActive[static_cast<size_t>(row)]; }
};

} // namespace arp
