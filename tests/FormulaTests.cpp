#include "Check.h"
#include "engine/Formula.h"
#include "sheet/Column.h"

#include <cmath>
#include <string>

using namespace arp::formula;
using arp::kMaxColumns;

namespace {

double eval(const char* src, Context ctx = {})
{
    std::string err;
    auto prog = Program::compile(src, err, kMaxColumns);
    CHECK(err.empty());
    if (!err.empty())
        std::fprintf(stderr, "  compile error for '%s': %s\n", src, err.c_str());
    return prog.eval(ctx);
}

bool fails(const char* src)
{
    std::string err;
    auto prog = Program::compile(src, err, kMaxColumns);
    return !err.empty() && prog.empty();
}

void testArithmetic()
{
    CHECK(eval("=1+2*3") == 7);
    CHECK(eval("(1+2)*3") == 9);
    CHECK(eval("=2^3^2") == 512); // right-associative
    CHECK(eval("=-2^2") == 4);    // Excel: unary minus binds tighter
    CHECK(eval("=10/4") == 2.5);
    CHECK(eval("=1/0") == 0);     // no inf/NaN reaches MIDI
    CHECK(eval("=7%3") == 1);
    CHECK(eval("=1.5e1") == 15);
}

void testComparisonsAndFunctions()
{
    CHECK(eval("=1<2") == 1);
    CHECK(eval("=2<>2") == 0);
    CHECK(eval("=3>=3") == 1);
    CHECK(eval("=IF(1=1,60,62)") == 60);
    CHECK(eval("=MOD(-1,4)") == 3); // Excel semantics, not fmod
    CHECK(eval("=SUM(1,2,3)") == 6);
    CHECK(eval("=AVERAGE(2,4)") == 3);
    CHECK(eval("=MIN(5,2,9)") == 2);
    CHECK(eval("=MAX(5,2,9)") == 9);
    CHECK(eval("=ABS(-3)") == 3);
    CHECK(eval("=ROUND(2.345,2)") == 2.35);
    CHECK(eval("=FLOOR(2.7)+CEIL(2.1)") == 5);
    CHECK(eval("=TRUE+FALSE") == 1);
    CHECK(eval("=mod(5,3)") == 2); // case-insensitive
}

void testVariablesAndCells()
{
    double cells[kMaxColumns * kCellRows] = {};
    cells[0] = 60;             // A1
    cells[kCellRows + 1] = 7;  // B2
    Context ctx;
    ctx.cells = cells;
    ctx.set(Var::Note, 64);
    ctx.set(Var::Step, 5);
    ctx.set(Var::Velocity, 90);
    CHECK(eval("=NOTE+7", ctx) == 71);
    CHECK(eval("=A1+B2", ctx) == 67);
    CHECK(eval("=IF(MOD(STEP,4)=1,VELOCITY-10,VELOCITY)", ctx) == 80);
}

void testMultiLetterCellRefs()
{
    double cells[kMaxColumns * kCellRows] = {};
    cells[26 * kCellRows + 0] = 42; // AA1 (column 26)
    cells[27 * kCellRows + 1] = 8;  // AB2 (column 27)
    Context ctx;
    ctx.cells = cells;
    CHECK(eval("=AA1", ctx) == 42);
    CHECK(eval("=AB2", ctx) == 8);
    CHECK(eval("=AA1+AB2", ctx) == 50);
}

void testAbsoluteRefs()
{
    double cells[kMaxColumns * kCellRows] = {};
    cells[0] = 10; // A1
    Context ctx;
    ctx.cells = cells;
    CHECK(eval("=$A$1", ctx) == 10);
    CHECK(eval("=$A1+A$1", ctx) == 20); // $ is optional syntax only
}

void testNameRefs()
{
    double cells[kMaxColumns * kCellRows] = {};
    cells[0] = 60;          // Note row 0
    cells[kCellRows] = 7;   // Shift row 0
    cells[2 * kCellRows] = 1; // Octave row 0
    Context ctx;
    ctx.cells = cells;
    ctx.set(Var::Step, 0);
    std::vector<std::string> names = {"Note", "Shift", "Octave"};
    std::string err;
    auto prog = Program::compile("=Note[ROW]+Shift[STEP]+Octave[0]*12", err, kMaxColumns, &names);
    CHECK(err.empty());
    if (!err.empty())
        std::fprintf(stderr, "  name ref compile error: %s\n", err.c_str());
    CHECK(prog.eval(ctx) == 60 + 7 + 12);

    // Case-insensitive column names.
    prog = Program::compile("=note[row]", err, kMaxColumns, &names);
    CHECK(err.empty());
    CHECK(prog.eval(ctx) == 60);

    // Unknown column name is a compile error.
    prog = Program::compile("=Bogus[0]", err, kMaxColumns, &names);
    CHECK(!err.empty());
    // Without a name list, name refs are an error too.
    auto p2 = Program::compile("=Note[0]", err, kMaxColumns);
    CHECK(!err.empty());
}

void testNoteNames()
{
    // C4 = 60 reference point.
    CHECK(eval("=NOTE(\"C4\")") == 60);
    CHECK(eval("=NOTE(\"A4\")") == 69);
    CHECK(eval("=NOTE(\"C3\")") == 48);
    CHECK(eval("=NOTE(\"F#4\")") == 66);
    CHECK(eval("=NOTE(\"Bb2\")") == 46);
    CHECK(eval("=NOTE(\"A0\")") == 21);
    CHECK(eval("=NOTE(\"Bb2\")+NOTE(\"C4\")") == 106);

    // Zero-arg NOTE()/PREV() mirror the variables.
    Context ctx;
    ctx.set(Var::Note, 64);
    ctx.set(Var::Prev, 60);
    CHECK(eval("=NOTE()+2", ctx) == 66);
    CHECK(eval("=PREV()+12", ctx) == 72);

    // RAND aliases RANDOM.
    uint32_t rng = 7;
    Context r2;
    r2.rng = &rng;
    CHECK(eval("=RAND()", r2) >= 0.0);
    CHECK(eval("=RAND(1,2)", r2) >= 1.0);
}

void testRandom()
{
    uint32_t rng = 12345;
    Context ctx;
    ctx.rng = &rng;
    std::string err;
    auto prog = Program::compile("=RANDOM(60,72)", err, kMaxColumns);
    bool varied = false;
    double first = prog.eval(ctx);
    for (int i = 0; i < 100; ++i) {
        double v = prog.eval(ctx);
        CHECK(v >= 60 && v < 72);
        varied |= v != first;
    }
    CHECK(varied);
}

void testErrors()
{
    // These used to evaluate silently to 0.
    CHECK(fails("=FOO+1"));
    CHECK(fails("=NOPE(1)"));
    CHECK(fails("=MOD(1)"));
    CHECK(fails("=(1+2"));
    CHECK(fails("=1+"));
    CHECK(fails("=1 2"));
    CHECK(fails("=A65"));
    CHECK(fails(""));
    CHECK(fails(std::string(200, '(').c_str()));
}

} // namespace

void runFormulaTests()
{
    testArithmetic();
    testComparisonsAndFunctions();
    testVariablesAndCells();
    testMultiLetterCellRefs();
    testRandom();
    testErrors();
    testAbsoluteRefs();
    testNameRefs();
    testNoteNames();
}
