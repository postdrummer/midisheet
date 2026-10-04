#pragma once

// Excel-style formulas, compiled once (message thread) and evaluated many
// times (audio thread) without allocating. No JUCE dependency.
//
// Syntax (leading '=' optional):
//   numbers, TRUE/FALSE, cell refs A1..Z1, AA1..BL64 (column letters + row)
//   $-prefixed refs ($A$1, $A1, A$1) are accepted and bound absolutely,
//   i.e. they behave exactly like A1 (there is no copy/paste in the sheet)
//   name refs  Note[ROW], Shift[STEP], Octave[0] — a column by name plus a
//   0-based row (matching STEP); resolves through the sheet's column names
//   variables  STEP/ROW NOTE VELOCITY LENGTH CHANNEL PREV RANDOM/RAND
//   operators  + - * / % ^   comparisons = <> < > <= >=
//   functions  MOD IF SUM AVG/AVERAGE MIN MAX ABS ROUND FLOOR CEIL
//              RANDOM/RAND(min,max) NOTE()/PREV() (zero-arg aliases)
//              NOTE("C3") — parse a note name into a MIDI note number
//
// Cell references (letter or name based) resolve through the callback chain,
// which enforces: hidden column -> column default, forward reference
// (target column index > the source cell's column) -> column default, and
// the kMaxRefDepth depth cap. The compiler runs on the UI thread; eval() is
// zero-allocation and bounded on the audio thread.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace arp::formula {

// Parse "C3", "F#4", "Bb2", "A0", "c#-1"... into a MIDI note number
// (C4 = 60). Returns -1 when the text is not a note name or the result
// falls outside 0..127.
double parseNoteName(std::string_view s);

enum class Var : uint8_t { Step, Note, Velocity, Length, Channel, Prev, Count };

constexpr int kCellRows = 64; // 1..64

struct Context {
    double vars[static_cast<int>(Var::Count)] = {};
    const double* cells = nullptr; // kCellCols * kCellRows, column-major (A1, A2, ...)
    uint32_t* rng = nullptr;       // xorshift state, advanced by RANDOM

    // Optional cell-reference callback: `cellRef(sheet, index, ctx)` returns
    // the *computed* value of cell `index` (formula > value > column default)
    // instead of the raw literal in `cells`. CompiledSheet::evaluateCell
    // installs it; when it is null (plain evaluation) Op::Cell falls back to
    // `cells`.
    const void* sheet = nullptr;
    double (*cellRef)(const void* sheet, int index, const Context& ctx) = nullptr;

    // Reference-chain depth, bumped by the callback so circular references
    // (A1 = B1, B1 = A1) terminate instead of recursing forever.
    int depth = 0;

    // Column index of the cell whose program is currently evaluating, or -1
    // when unknown. The reference callback uses it to reject forward
    // references: a cell may only read columns to its left (or its own).
    int srcCol = -1;

    void set(Var v, double value) { vars[static_cast<int>(v)] = value; }
};

class Program {
public:
    // Returns an empty program and sets `error` on failure. `numCols` is the
    // sheet's column count, used to resolve cell references (A1, AA1, ...).
    // `columnNames` (optional) enables name-based refs like Note[ROW]; match
    // is case-insensitive against the sheet's column names.
    static Program compile(std::string_view source, std::string& error, int numCols,
                           const std::vector<std::string>* columnNames = nullptr);

    bool empty() const { return nodes.empty(); }
    double eval(const Context& ctx) const; // real-time safe

private:
    enum class Op : uint8_t {
        Const, Var, Cell, CellAt, Random,
        Neg, Add, Sub, Mul, Div, Mod, Pow,
        Eq, Ne, Lt, Gt, Le, Ge,
        If, Sum, Avg, Min, Max, Abs, Round, Floor, Ceil, RandRange,
    };

    struct Node {
        Op op = Op::Const;
        double value = 0.0;  // Const
        int index = 0;       // Var / Cell index
        int firstArg = 0;    // into `args`
        int numArgs = 0;
    };

    double evalNode(int node, const Context& ctx) const;

    std::vector<Node> nodes; // root is the last node
    std::vector<int> args;   // child node indices

    friend class Parser;
};

} // namespace arp::formula
