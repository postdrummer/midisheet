#pragma once

// Excel-style formulas, compiled once (message thread) and evaluated many
// times (audio thread) without allocating. No JUCE dependency.
//
// Syntax (leading '=' optional):
//   numbers, TRUE/FALSE, cell refs A1..H64
//   variables  STEP NOTE VELOCITY LENGTH CHANNEL PREV RANDOM
//   operators  + - * / % ^   comparisons = <> < > <= >=
//   functions  MOD IF SUM AVG/AVERAGE MIN MAX ABS ROUND FLOOR CEIL RANDOM

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace arp::formula {

enum class Var : uint8_t { Step, Note, Velocity, Length, Channel, Prev, Count };

constexpr int kCellCols = 8;  // A..H
constexpr int kCellRows = 64; // 1..64

struct Context {
    double vars[static_cast<int>(Var::Count)] = {};
    const double* cells = nullptr; // kCellCols * kCellRows, column-major (A1, A2, ...)
    uint32_t* rng = nullptr;       // xorshift state, advanced by RANDOM

    void set(Var v, double value) { vars[static_cast<int>(v)] = value; }
};

class Program {
public:
    // Returns an empty program and sets `error` on failure.
    static Program compile(std::string_view source, std::string& error);

    bool empty() const { return nodes.empty(); }
    double eval(Context& ctx) const; // real-time safe

private:
    enum class Op : uint8_t {
        Const, Var, Cell, Random,
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

    double evalNode(int node, Context& ctx) const;

    std::vector<Node> nodes; // root is the last node
    std::vector<int> args;   // child node indices

    friend class Parser;
};

} // namespace arp::formula
