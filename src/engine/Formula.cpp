#include "Formula.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace arp::formula {

namespace {

constexpr int kMaxDepth = 64;

std::string upper(std::string_view s)
{
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

double nextRandom(uint32_t* rng)
{
    if (rng == nullptr)
        return 0.5;
    uint32_t x = *rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *rng = x;
    return static_cast<double>(x) / 4294967296.0; // [0, 1)
}

} // namespace

// Recursive-descent parser emitting nodes in post-order.
class Parser {
public:
    Parser(std::string_view src, Program& prog, int numCols) : s(src), p(prog), cols(numCols) {}

    bool run(std::string& error)
    {
        skipSpace();
        if (pos < s.size() && s[pos] == '=')
            ++pos;
        skipSpace();
        if (pos >= s.size())
            return fail("Empty formula", error);
        comparison(0);
        skipSpace();
        if (err.empty() && pos < s.size())
            err = "Unexpected '" + std::string(1, s[pos]) + "'";
        if (!err.empty())
            return fail(err, error);
        return true;
    }

private:
    using Op = Program::Op;

    bool fail(const std::string& msg, std::string& error)
    {
        error = msg;
        p.nodes.clear();
        p.args.clear();
        return false;
    }

    void skipSpace()
    {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
            ++pos;
    }

    bool accept(std::string_view tok)
    {
        skipSpace();
        if (s.substr(pos, tok.size()) == tok) {
            pos += tok.size();
            return true;
        }
        return false;
    }

    int emit(Program::Node n)
    {
        p.nodes.push_back(n);
        return static_cast<int>(p.nodes.size()) - 1;
    }

    int emitOp(Op op, std::initializer_list<int> children)
    {
        Program::Node n;
        n.op = op;
        n.firstArg = static_cast<int>(p.args.size());
        n.numArgs = static_cast<int>(children.size());
        for (int c : children)
            p.args.push_back(c);
        return emit(n);
    }

    int error(const std::string& msg)
    {
        if (err.empty())
            err = msg;
        return emit({});
    }

    int comparison(int depth)
    {
        if (depth > kMaxDepth)
            return error("Formula nested too deeply");
        int left = additive(depth);
        for (;;) {
            Op op;
            if (accept("<>")) op = Op::Ne;
            else if (accept("<=")) op = Op::Le;
            else if (accept(">=")) op = Op::Ge;
            else if (accept("=")) op = Op::Eq;
            else if (accept("<")) op = Op::Lt;
            else if (accept(">")) op = Op::Gt;
            else return left;
            left = emitOp(op, {left, additive(depth)});
        }
    }

    int additive(int depth)
    {
        int left = multiplicative(depth);
        for (;;) {
            if (accept("+")) left = emitOp(Op::Add, {left, multiplicative(depth)});
            else if (accept("-")) left = emitOp(Op::Sub, {left, multiplicative(depth)});
            else return left;
        }
    }

    int multiplicative(int depth)
    {
        int left = power(depth);
        for (;;) {
            if (accept("*")) left = emitOp(Op::Mul, {left, power(depth)});
            else if (accept("/")) left = emitOp(Op::Div, {left, power(depth)});
            else if (accept("%")) left = emitOp(Op::Mod, {left, power(depth)});
            else return left;
        }
    }

    int power(int depth)
    {
        int base = unary(depth);
        if (accept("^"))
            return emitOp(Op::Pow, {base, power(depth + 1)}); // right-associative
        return base;
    }

    int unary(int depth)
    {
        if (depth > kMaxDepth)
            return error("Formula nested too deeply");
        if (accept("-"))
            return emitOp(Op::Neg, {unary(depth + 1)});
        if (accept("+"))
            return unary(depth + 1);
        return primary(depth);
    }

    int primary(int depth)
    {
        skipSpace();
        if (pos >= s.size())
            return error("Unexpected end of formula");

        const char c = s[pos];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            const char* begin = s.data() + pos;
            char* end = nullptr;
            double v = std::strtod(begin, &end);
            if (end == begin)
                return error("Bad number");
            pos += static_cast<size_t>(end - begin);
            Program::Node n;
            n.value = v;
            return emit(n);
        }

        if (accept("(")) {
            int inner = comparison(depth + 1);
            if (!accept(")"))
                return error("Missing ')'");
            return inner;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = pos;
            while (pos < s.size() && (std::isalnum(static_cast<unsigned char>(s[pos])) || s[pos] == '_'))
                ++pos;
            return identifier(upper(s.substr(start, pos - start)), depth);
        }

        return error("Unexpected '" + std::string(1, c) + "'");
    }

    int identifier(const std::string& name, int depth)
    {
        skipSpace();
        if (pos < s.size() && s[pos] == '(') {
            ++pos;
            return function(name, depth);
        }

        static const struct { const char* name; Var var; } kVars[] = {
            {"STEP", Var::Step}, {"NOTE", Var::Note}, {"VELOCITY", Var::Velocity},
            {"LENGTH", Var::Length}, {"CHANNEL", Var::Channel}, {"PREV", Var::Prev},
        };
        for (auto& v : kVars) {
            if (name == v.name) {
                Program::Node n;
                n.op = Op::Var;
                n.index = static_cast<int>(v.var);
                return emit(n);
            }
        }
        if (name == "TRUE" || name == "FALSE") {
            Program::Node n;
            n.value = name == "TRUE" ? 1.0 : 0.0;
            return emit(n);
        }
        if (name == "RANDOM")
            return emitOp(Op::Random, {});

        // Cell reference: column letters (A..Z, AA..CL) then a row 1..64.
        size_t letterEnd = 0;
        while (letterEnd < name.size() && name[letterEnd] >= 'A' && name[letterEnd] <= 'Z')
            ++letterEnd;
        if (letterEnd > 0 && letterEnd < name.size()) {
            bool digits = true;
            for (size_t i = letterEnd; i < name.size(); ++i)
                digits &= std::isdigit(static_cast<unsigned char>(name[i])) != 0;
            if (digits) {
                int col = 0;
                for (size_t i = 0; i < letterEnd; ++i)
                    col = col * 26 + (name[static_cast<int>(i)] - 'A' + 1);
                --col; // 0-based
                int row = std::atoi(name.c_str() + static_cast<int>(letterEnd));
                if (col < 0 || col >= cols || row < 1 || row > kCellRows)
                    return error("Cell out of range: " + name);
                Program::Node n;
                n.op = Op::Cell;
                n.index = col * kCellRows + (row - 1);
                return emit(n);
            }
        }
        return error("Unknown name: " + name);
    }

    int function(const std::string& name, int depth)
    {
        std::vector<int> argv;
        if (!accept(")")) {
            do {
                argv.push_back(comparison(depth + 1));
            } while (accept(","));
            if (!accept(")"))
                return error("Missing ')' after " + name + " arguments");
        }
        const auto n = argv.size();

        struct Fn { const char* name; Op op; size_t minArgs, maxArgs; };
        static const Fn kFns[] = {
            {"IF", Op::If, 3, 3},       {"MOD", Op::Mod, 2, 2},
            {"SUM", Op::Sum, 1, 64},    {"AVG", Op::Avg, 1, 64},
            {"AVERAGE", Op::Avg, 1, 64}, {"MIN", Op::Min, 1, 64},
            {"MAX", Op::Max, 1, 64},    {"ABS", Op::Abs, 1, 1},
            {"ROUND", Op::Round, 1, 2}, {"FLOOR", Op::Floor, 1, 1},
            {"CEIL", Op::Ceil, 1, 1},
        };
        if (name == "RANDOM") {
            if (n == 0) return emitOp(Op::Random, {});
            if (n == 2) return emitOp(Op::RandRange, {argv[0], argv[1]});
            return error("RANDOM takes 0 or 2 arguments");
        }
        for (auto& f : kFns) {
            if (name != f.name)
                continue;
            if (n < f.minArgs || n > f.maxArgs)
                return error("Wrong number of arguments to " + name);
            Program::Node node;
            node.op = f.op;
            node.firstArg = static_cast<int>(p.args.size());
            node.numArgs = static_cast<int>(n);
            p.args.insert(p.args.end(), argv.begin(), argv.end());
            return emit(node);
        }
        return error("Unknown function: " + name);
    }

    std::string s; // owned copy: null-terminated for strtod
    Program& p;
    int cols; // sheet column count for cell reference resolution
    size_t pos = 0;
    std::string err;
};

Program Program::compile(std::string_view source, std::string& error, int numCols)
{
    error.clear();
    Program prog;
    Parser(source, prog, numCols).run(error);
    return prog;
}

double Program::eval(const Context& ctx) const
{
    if (nodes.empty())
        return 0.0;
    double v = evalNode(static_cast<int>(nodes.size()) - 1, ctx);
    return std::isfinite(v) ? v : 0.0;
}

double Program::evalNode(int i, const Context& ctx) const
{
    const Node& n = nodes[static_cast<size_t>(i)];
    auto arg = [&](int k) { return evalNode(args[static_cast<size_t>(n.firstArg + k)], ctx); };

    switch (n.op) {
    case Op::Const: return n.value;
    case Op::Var: return ctx.vars[n.index];
    case Op::Cell:
        if (ctx.cellRef != nullptr)
            return ctx.cellRef(ctx.sheet, n.index, ctx); // computed value of the cell
        return ctx.cells ? ctx.cells[n.index] : 0.0;
    case Op::Random: return nextRandom(ctx.rng);
    case Op::RandRange: {
        double lo = arg(0), hi = arg(1);
        return lo + (hi - lo) * nextRandom(ctx.rng);
    }
    case Op::Neg: return -arg(0);
    case Op::Add: return arg(0) + arg(1);
    case Op::Sub: return arg(0) - arg(1);
    case Op::Mul: return arg(0) * arg(1);
    case Op::Div: { double d = arg(1); return d != 0.0 ? arg(0) / d : 0.0; }
    case Op::Mod: {
        // Excel semantics: result takes the sign of the divisor.
        double a = arg(0), b = arg(1);
        return b != 0.0 ? a - b * std::floor(a / b) : 0.0;
    }
    case Op::Pow: return std::pow(arg(0), arg(1));
    case Op::Eq: return arg(0) == arg(1) ? 1.0 : 0.0;
    case Op::Ne: return arg(0) != arg(1) ? 1.0 : 0.0;
    case Op::Lt: return arg(0) < arg(1) ? 1.0 : 0.0;
    case Op::Gt: return arg(0) > arg(1) ? 1.0 : 0.0;
    case Op::Le: return arg(0) <= arg(1) ? 1.0 : 0.0;
    case Op::Ge: return arg(0) >= arg(1) ? 1.0 : 0.0;
    case Op::If: return arg(0) != 0.0 ? arg(1) : arg(2); // only the taken branch runs
    case Op::Sum:
    case Op::Avg: {
        double sum = 0.0;
        for (int k = 0; k < n.numArgs; ++k)
            sum += arg(k);
        return n.op == Op::Avg ? sum / n.numArgs : sum;
    }
    case Op::Min:
    case Op::Max: {
        double m = arg(0);
        for (int k = 1; k < n.numArgs; ++k) {
            double v = arg(k);
            m = n.op == Op::Min ? std::min(m, v) : std::max(m, v);
        }
        return m;
    }
    case Op::Abs: return std::abs(arg(0));
    case Op::Round: {
        double f = std::pow(10.0, n.numArgs > 1 ? arg(1) : 0.0);
        return std::round(arg(0) * f) / f;
    }
    case Op::Floor: return std::floor(arg(0));
    case Op::Ceil: return std::ceil(arg(0));
    }
    return 0.0;
}

} // namespace arp::formula
