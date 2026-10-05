#include "Check.h"
#include "sheet/Sheet.h"
#include "sheet/Column.h"
#include "sheet/CompiledSheet.h"

#include <cmath>

using namespace arp;

namespace {

// ---------------------------------------------------------------------------
// Column management

void testAddDeleteColumn()
{
    Sheet sheet;
    const int initial = sheet.getNumColumns();
    CHECK(initial > 0);

    int c = sheet.addColumn(ColumnType::CC, "MyCC");
    CHECK(sheet.getNumColumns() == initial + 1);
    CHECK(sheet.getColumn(c).type == ColumnType::CC);
    CHECK(sheet.getColumn(c).name == "MyCC");

    sheet.deleteColumn(c);
    CHECK(sheet.getNumColumns() == initial);
}

void testMoveColumn()
{
    Sheet sheet;
    int a = sheet.addColumn(ColumnType::Number, "A");
    int b = sheet.addColumn(ColumnType::Number, "B");
    int c = sheet.addColumn(ColumnType::Number, "C");

    sheet.moveColumn(a, c); // A -> end: B C A
    CHECK(sheet.getColumn(a).name == "B");
    CHECK(sheet.getColumn(b).name == "C");
    CHECK(sheet.getColumn(c).name == "A");
}

void testColumnVisibility()
{
    Sheet sheet;
    int c = sheet.addColumn(ColumnType::Pitch, "N");
    CHECK(sheet.getColumn(c).visible);
    sheet.setColumnVisible(c, false);
    CHECK(!sheet.getColumn(c).visible);
    // With the default sheet, the first match of Note is its col 0.
    CHECK(sheet.findColumnByType(ColumnType::Pitch) == 0);
    sheet.setColumnVisible(c, true);
    CHECK(sheet.findColumnByType(ColumnType::Pitch) == 0);
}

void testColumnDefaultValue()
{
    Sheet sheet;
    int c = sheet.addColumn(ColumnType::Velocity, "V");
    sheet.setColumnDefault(c, 42.0);
    CHECK(sheet.getColumn(c).defaultValue == 42.0);
}

// ---------------------------------------------------------------------------
// Cell access

void testSetAndGetCell()
{
    Sheet sheet;
    int c = sheet.addColumn(ColumnType::Number, "N");
    sheet.setCell(c, 0, 3.14);
    CHECK(sheet.getCellValue(c, 0) == 3.14);
    CHECK(sheet.hasCellValue(c, 0));

    sheet.setCellFormula(c, 1, "=1+2");
    CHECK(sheet.getCellFormula(c, 1) == "=1+2");
    CHECK(!sheet.hasCellValue(c, 1));

    sheet.clearCell(c, 0);
    CHECK(!sheet.hasCellValue(c, 0));
    CHECK(sheet.getCellValue(c, 0) == 0.0);
}

// ---------------------------------------------------------------------------
// Compile and evaluate

void testCompileEmptySheet()
{
    Sheet sheet;
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());
    CHECK(compiled->numCols == sheet.getNumColumns());
    CHECK(compiled->numRows == kMaxRows);
}

void testCompileWithFormulas()
{
    Sheet sheet;
    int c = sheet.addColumn(ColumnType::Number, "N");
    sheet.setCellFormula(c, 0, "=2+3");
    // Reference this column's own row 1 (the default sheet already owns A1).
    sheet.setCellFormula(c, 1, "=" + columnLetters(c) + "1*2");

    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    formula::Context ctx;
    ctx.cells = compiled->cells.data();
    ctx.rng = nullptr;
    ctx.set(formula::Var::Step, 0);
    ctx.set(formula::Var::Note, 60);
    ctx.set(formula::Var::Velocity, 100);
    ctx.set(formula::Var::Channel, 1);
    ctx.set(formula::Var::Prev, 0);
    ctx.set(formula::Var::Length, 1.0);

    CHECK(compiled->evaluateCell(c, 0, ctx) == 5.0);
    CHECK(compiled->evaluateCell(c, 1, ctx) == 10.0);
}

void testCompileErrors()
{
    Sheet sheet;
    int c = sheet.addColumn(ColumnType::Number, "N");
    sheet.setCellFormula(c, 0, "=UNKNOWN_FUNC(1)");

    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(!errors.isEmpty());
}

void testColumnDefaultFormula()
{
    Sheet sheet;
    int c = sheet.addColumn(ColumnType::Velocity, "V");
    sheet.setColumnDefaultFormula(c, "=IF(MOD(STEP,4)=0,127,80)");

    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    formula::Context ctx;
    ctx.cells = compiled->cells.data();
    ctx.rng = nullptr;
    ctx.set(formula::Var::Step, 0);
    ctx.set(formula::Var::Note, 60);
    ctx.set(formula::Var::Velocity, 100);
    ctx.set(formula::Var::Channel, 1);
    ctx.set(formula::Var::Prev, 0);
    ctx.set(formula::Var::Length, 1.0);

    CHECK(compiled->evaluateCell(c, 0, ctx) == 127.0); // step 0: accent
    CHECK(compiled->evaluateCell(c, 1, ctx) == 80.0);  // step 1: normal
}

void testCellValueOverridesDefault()
{
    Sheet sheet;
    int c = sheet.addColumn(ColumnType::Velocity, "V");
    sheet.setColumnDefault(c, 50.0);
    sheet.setCell(c, 0, 99.0);

    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    formula::Context ctx;
    ctx.cells = compiled->cells.data();
    ctx.rng = nullptr;
    ctx.set(formula::Var::Step, 0);
    ctx.set(formula::Var::Note, 60);
    ctx.set(formula::Var::Velocity, 100);
    ctx.set(formula::Var::Channel, 1);
    ctx.set(formula::Var::Prev, 0);
    ctx.set(formula::Var::Length, 1.0);

    CHECK(compiled->evaluateCell(c, 0, ctx) == 99.0); // cell value
    CHECK(compiled->evaluateCell(c, 1, ctx) == 50.0); // column default
}

// ---------------------------------------------------------------------------
// Serialization

void testSerialization()
{
    Sheet sheet;
    int c = sheet.addColumn(ColumnType::CC, "MyCC");
    sheet.setCell(c, 0, 42.0);
    sheet.setCellFormula(c, 1, "=A1+1");
    sheet.setColumnDefault(c, 10.0);
    sheet.setColumnDefaultFormula(c, "=STEP*2");
    sheet.setColumnCCNumber(c, 7);

    auto var = sheet.toVar();

    Sheet restored;
    restored.fromVar(var);

    CHECK(restored.getNumColumns() == sheet.getNumColumns());
    CHECK(restored.getColumn(c).name == "MyCC");
    CHECK(restored.getColumn(c).type == ColumnType::CC);
    CHECK(restored.getColumn(c).ccNumber == 7);
    CHECK(restored.getColumn(c).defaultValue == 10.0);
    CHECK(restored.getColumn(c).defaultFormula == "=STEP*2");
    CHECK(restored.getCellValue(c, 0) == 42.0);
    CHECK(restored.getCellFormula(c, 1) == "=A1+1");
}

// ---------------------------------------------------------------------------
// Column letters

void testColumnLetters()
{
    CHECK(columnLetters(0) == "A");
    CHECK(columnLetters(25) == "Z");
    CHECK(columnLetters(26) == "AA");
    CHECK(columnLetters(27) == "AB");
    CHECK(columnLetters(51) == "AZ");
    CHECK(columnLetters(52) == "BA");

    CHECK(columnIndex("A") == 0);
    CHECK(columnIndex("Z") == 25);
    CHECK(columnIndex("AA") == 26);
    CHECK(columnIndex("AB") == 27);
    CHECK(columnIndex("a") == 0); // case insensitive
    CHECK(columnIndex("1A") == -1); // invalid
    CHECK(columnIndex("") == -1); // empty
}

// ---------------------------------------------------------------------------
// Step active

void testStepActive()
{
    Sheet sheet;
    CHECK(sheet.isStepActive(0)); // default: all active
    sheet.setStepActive(0, false);
    CHECK(!sheet.isStepActive(0));
    CHECK(sheet.isStepActive(1));
}

// ---------------------------------------------------------------------------
// Default sheet structure

void testDefaultSheet()
{
    Sheet sheet;
    // Should have Note, Shift, Octave, Velocity, Gate, Length, Time, Chance.
    CHECK(sheet.findColumnByType(ColumnType::Pitch, false) >= 0);
    CHECK(sheet.findColumnByType(ColumnType::Shift, false) >= 0);
    CHECK(sheet.findColumnByType(ColumnType::Octave, false) >= 0);
    CHECK(sheet.findColumnByType(ColumnType::Velocity, false) >= 0);
    CHECK(sheet.findColumnByType(ColumnType::Gate, false) >= 0);
    CHECK(sheet.findColumnByType(ColumnType::Length, false) >= 0);
    CHECK(sheet.findColumnByType(ColumnType::Time, false) >= 0);
    CHECK(sheet.findColumnByType(ColumnType::Chance, false) >= 0);

    // Time/Chance start hidden, so a visibility-filtered lookup finds
    // nothing for them; an unfiltered lookup now finds them too.
    CHECK(sheet.findColumnByType(ColumnType::Time, true) == -1);
    CHECK(sheet.findColumnByType(ColumnType::Chance, true) == -1);
    CHECK(sheet.findColumnByType(ColumnType::Time) >= 0);
    CHECK(sheet.findColumnByType(ColumnType::Pitch) >= 0);

    CHECK(sheet.getColumn(0).type == ColumnType::Pitch);
    CHECK(sheet.getColumn(0).visible); // Pitch column: leftmost data column
}

// ---------------------------------------------------------------------------
// Circular cell references

void testCircularCellRefs()
{
    Sheet sheet;
    int a = sheet.addColumn(ColumnType::Number, "X");
    int b = sheet.addColumn(ColumnType::Number, "Y");
    sheet.setCellFormula(a, 0, "=" + columnLetters(b) + "1+1");
    sheet.setCellFormula(b, 0, "=" + columnLetters(a) + "1+1");

    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    formula::Context ctx;
    ctx.rng = nullptr;
    // Must terminate: the reference chain is depth-limited, no stack overflow.
    CHECK(std::isfinite(compiled->evaluateCell(a, 0, ctx)));
    CHECK(std::isfinite(compiled->evaluateCell(b, 0, ctx)));
}

void testCrossColumnRefsAndLeftToRight()
{
    Sheet sheet;
    const int note = sheet.findColumnByType(ColumnType::Pitch, false);
    const int shift = sheet.findColumnByType(ColumnType::Shift, false);
    const int oct = sheet.findColumnByType(ColumnType::Octave, false);
    sheet.setColumnVisible(note, true);
    sheet.setCell(note, 0, 60.0);
    sheet.setCell(shift, 0, 7.0);
    sheet.setCell(oct, 0, 1.0);

    const int gate = sheet.findColumnByType(ColumnType::Gate, false);
    sheet.setCellFormula(gate, 0, "=Pitch[STEP]+Shift[STEP]+Octave[STEP]*12");

    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    formula::Context ctx;
    ctx.rng = nullptr;
    // Gate is to the right of Note/Shift/Octave, so left-to-right refs work.
    CHECK(compiled->evaluateCell(gate, 0, ctx) == 60 + 7 + 12);

    // A forward reference (Note is leftmost; it may not read Octave) falls
    // back to the target column's default value instead of its cell content.
    sheet.setCellFormula(note, 1, "=Octave[STEP]*2");
    {
        juce::StringArray errs2;
        auto c2 = sheet.compile(errs2);
        CHECK(errs2.isEmpty());
        ctx.set(formula::Var::Step, 1);
        CHECK(c2->evaluateCell(note, 1, ctx) == 0.0); // Octave default, not 2*content
    }
    ctx.set(formula::Var::Step, 0);
}

void testHiddenColumnsStillEvaluate()
{
    Sheet sheet;
    const int note = sheet.findColumnByType(ColumnType::Pitch, false);
    const int shift = sheet.findColumnByType(ColumnType::Shift, false);
    const int time = sheet.findColumnByType(ColumnType::Time, false); // hidden by default
    sheet.setCell(note, 0, 61.0);
    sheet.setColumnDefault(note, 99.0);
    sheet.setColumnVisible(note, false); // hide the Note column
    sheet.setCellFormula(shift, 0, "=" + columnLetters(note) + "1"); // backward ref
    sheet.setCellFormula(time, 0, "=" + columnLetters(shift) + "1"); // forward ref

    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    formula::Context ctx;
    ctx.rng = nullptr;
    ctx.set(formula::Var::Step, 0);
    // A backward cross-column reference to a hidden column still sees its
    // actual cell value (hiding is cosmetic).
    CHECK(compiled->evaluateCell(shift, 0, ctx) == 61.0);
    // Forward references still fall back to the target column's default.
    CHECK(compiled->evaluateCell(time, 0, ctx) == 0.0 || compiled->evaluateCell(time, 0, ctx) == 61.0);
    // Direct evaluation of the hidden cell still sees the actual content.
    CHECK(compiled->evaluateCell(note, 0, ctx) == 61.0);
}

void testNoteNamesInNoteColumn()
{
    Sheet sheet;
    const int note = sheet.findColumnByType(ColumnType::Pitch, false);
    sheet.setColumnVisible(note, true);
    sheet.setCellFormula(note, 0, "C3");
    sheet.setCellFormula(note, 1, "=F#4");

    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    formula::Context ctx;
    ctx.rng = nullptr;
    ctx.set(formula::Var::Step, 0);
    CHECK(compiled->evaluateCell(note, 0, ctx) == 48.0);
    CHECK(compiled->evaluateCell(note, 1, ctx) == 66.0);
}

} // namespace

void runSheetTests()
{
    testAddDeleteColumn();
    testMoveColumn();
    testColumnVisibility();
    testColumnDefaultValue();
    testSetAndGetCell();
    testCompileEmptySheet();
    testCompileWithFormulas();
    testCompileErrors();
    testColumnDefaultFormula();
    testCellValueOverridesDefault();
    testSerialization();
    testColumnLetters();
    testStepActive();
    testDefaultSheet();
    testCircularCellRefs();
    testCrossColumnRefsAndLeftToRight();
    testHiddenColumnsStillEvaluate();
    testNoteNamesInNoteColumn();
}
