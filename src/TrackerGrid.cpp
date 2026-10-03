#include "TrackerGrid.h"

namespace {

constexpr int kRowH = 20;
constexpr int kHeaderH = 30; // two lines: type + name
constexpr int kIndexW = 34;
constexpr int kOnW = 32;
constexpr size_t kMaxUndo = 200;

const juce::Colour kBg(0xff16181c), kBeat(0xff1d2026), kGridLine(0xff262a31), kText(0xffd6dbe2),
    kDim(0xff646c78), kCursor(0xffe0b050), kPlay(0xff2c4a3a), kError(0xffff7a59), kInactive(0xff101114);

// Preview input: a C major triad held at velocity 100, played in Up order.
constexpr int kPreviewChord[] = {60, 64, 67};

juce::Font mono(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size,
                                        bold ? juce::Font::bold : juce::Font::plain));
}

} // namespace

TrackerGrid::TrackerGrid(MidisheetAudioProcessor& p) : proc(p)
{
    setWantsKeyboardFocus(true);
    refresh();
    startTimerHz(30);
}

// ---------------------------------------------------------------------------
// Model access

std::string* TrackerGrid::formulaRef(int r, int c)
{
    auto& sheet = proc.getSheet();
    if (c < 0 || c >= sheet.getNumColumns() || r < 0 || r >= arp::kMaxRows)
        return nullptr;
    // Mutable pointer to the cell's formula text. The sheet is non-const here
    // (proc.getSheet()), so casting away the const of getCellFormula() is safe.
    return &const_cast<std::string&>(sheet.getCellFormula(c, r));
}

juce::String TrackerGrid::cellName() const
{
    return juce::String(arp::columnLetters(col)) + juce::String(row + 1);
}

juce::String TrackerGrid::cellFormula() const
{
    auto* self = const_cast<TrackerGrid*>(this);
    if (auto* f = self->formulaRef(row, col))
        return juce::String(*f);
    return proc.getSheet().isStepActive(row) ? "on" : "off";
}

juce::String TrackerGrid::cellError() const
{
    auto* self = const_cast<TrackerGrid*>(this);
    if (auto* f = self->formulaRef(row, col); f != nullptr && !juce::String(*f).trim().isEmpty()) {
        std::string err;
        arp::formula::Program::compile(*f, err, proc.getSheet().getNumColumns());
        return juce::String(err);
    }
    return {};
}

void TrackerGrid::edit(const std::function<void()>& change)
{
    undoStack.push_back(proc.getSheet());
    if (undoStack.size() > kMaxUndo)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    change();
    proc.sheetChanged();
    refresh();
}

void TrackerGrid::setCellFormula(const juce::String& text)
{
    auto& sheet = proc.getSheet();
    if (col < 0 || col >= sheet.getNumColumns())
        return;
    auto* f = formulaRef(row, col);
    if (f == nullptr || *f == text.trim().toStdString())
        return;
    edit([f, t = text.trim().toStdString()] { *f = t; });
}

void TrackerGrid::toggleStep(int r)
{
    edit([this, r] {
        auto& sheet = proc.getSheet();
        sheet.setStepActive(r, !sheet.isStepActive(r));
    });
}

void TrackerGrid::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back(proc.getSheet());
    proc.getSheet() = undoStack.back();
    undoStack.pop_back();
    proc.sheetChanged();
    refresh();
}

void TrackerGrid::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back(proc.getSheet());
    proc.getSheet() = redoStack.back();
    redoStack.pop_back();
    proc.sheetChanged();
    refresh();
}

// ---------------------------------------------------------------------------
// Preview

void TrackerGrid::refresh()
{
    seenVersion = proc.getSheetVersion();

    auto& sheet = proc.getSheet();
    const int numCols = sheet.getNumColumns();

    juce::StringArray ignored;
    const auto compiled = sheet.compile(ignored);
    const double defaultGate = proc.getAPVTS().getRawParameterValue("gate")->load();

    // Resize preview to [row][col].
    preview.assign(static_cast<size_t>(arp::kMaxRows), std::vector<Preview>(static_cast<size_t>(numCols)));

    uint32_t rng = 0x9e3779b9u;
    int prev = 0;
    for (int r = 0; r < arp::kMaxRows; ++r) {
        const int input = kPreviewChord[r % 3];
        const auto res = arp::evaluateStep(*compiled, r, {1, input, 100}, prev, defaultGate, &rng);
        if (res.playable && sheet.isStepActive(r))
            prev = res.pitch;

        for (int c = 0; c < numCols; ++c) {
            Preview pv;
            const auto& meta = sheet.getColumn(c);
            const juce::String& text = sheet.getCellFormula(c, r);
            pv.isDefault = text.trim().isEmpty() && !sheet.hasCellValue(c, r);
            pv.isRandom = text.containsIgnoreCase("RANDOM");
            if (!pv.isDefault && text.trim().isNotEmpty()) {
                std::string err;
                arp::formula::Program::compile(text.toStdString(), err, numCols);
                pv.isError = !err.empty();
            }
            if (pv.isError) {
                pv.text = "ERR";
            } else {
                // Show the evaluated value for this column.
                arp::formula::Context ctx;
                ctx.cells = compiled->cells.data();
                ctx.rng = &rng;
                ctx.set(arp::formula::Var::Step, r);
                ctx.set(arp::formula::Var::Note, input);
                ctx.set(arp::formula::Var::Velocity, 100);
                ctx.set(arp::formula::Var::Channel, 1);
                ctx.set(arp::formula::Var::Prev, prev);
                ctx.set(arp::formula::Var::Length, 1.0);
                const double val = compiled->evaluateCell(c, r, ctx);
                switch (meta.type) {
                    case arp::ColumnType::Note:
                        pv.text = val < 0 ? "---" : juce::MidiMessage::getMidiNoteName(static_cast<int>(val), true, true, 4);
                        break;
                    case arp::ColumnType::Velocity:
                    case arp::ColumnType::Chance:
                        pv.text = juce::String(juce::roundToInt(val));
                        break;
                    case arp::ColumnType::Gate:
                    case arp::ColumnType::Percent:
                        pv.text = juce::String(juce::roundToInt(val)) + "%";
                        break;
                    case arp::ColumnType::Length:
                        pv.text = juce::String(val, val == std::floor(val) ? 0 : 2);
                        break;
                    default:
                        pv.text = juce::String(val, val == std::floor(val) ? 0 : 2);
                        break;
                }
            }
            preview[static_cast<size_t>(r)][static_cast<size_t>(c)] = pv;
        }
    }
    repaint();
    if (onSelectionChanged)
        onSelectionChanged();
}

void TrackerGrid::timerCallback()
{
    if (proc.getSheetVersion() != seenVersion)
        refresh(); // e.g. host loaded a preset
    const int step = proc.getPlayingStep();
    if (step != seenStep) {
        seenStep = step;
        repaint();
    }
}

// ---------------------------------------------------------------------------
// Layout and painting

int TrackerGrid::numActiveSteps() const
{
    return static_cast<int>(proc.getAPVTS().getRawParameterValue("numSteps")->load());
}

int TrackerGrid::visibleRows() const
{
    return juce::jmax(1, (getHeight() - kHeaderH) / kRowH);
}

void TrackerGrid::resized()
{
    ensureVisible();
}

void TrackerGrid::ensureVisible()
{
    const int vis = visibleRows();
    if (row < scrollRow)
        scrollRow = row;
    else if (row >= scrollRow + vis)
        scrollRow = row - vis + 1;
    scrollRow = juce::jlimit(0, juce::jmax(0, arp::kMaxRows - vis), scrollRow);
}

// Returns the x offset of column c (0 = row index, 1 = on toggle, 2+ = sheet columns).
static int colX(int c, int width, int numCols)
{
    if (c == 0)
        return kIndexW;
    const int dataCols = numCols + 1; // +1 for the "on" column
    const int w = (width - kIndexW) / dataCols;
    return kIndexW + (c - 0) * w;
}

static int colW(int c, int width, int numCols)
{
    const int dataCols = numCols + 1;
    return (width - kIndexW) / dataCols;
}

bool TrackerGrid::cellAt(juce::Point<int> pt, int& r, int& c) const
{
    if (pt.y < kHeaderH || pt.x < kIndexW)
        return false;
    r = scrollRow + (pt.y - kHeaderH) / kRowH;
    if (r >= arp::kMaxRows)
        return false;
    const int numCols = proc.getSheet().getNumColumns();
    const int totalCols = numCols + 2; // index + on + data
    for (c = totalCols - 1; c > 0 && pt.x < colX(c, getWidth(), numCols); --c) {}
    return true;
}

void TrackerGrid::paint(juce::Graphics& g)
{
    const int w = getWidth();
    auto& sheet = proc.getSheet();
    const int numCols = sheet.getNumColumns();
    const int totalCols = numCols + 2; // index + on + data

    g.fillAll(kBg);

    // Column headers.
    g.setFont(mono(10.0f, true));
    g.setColour(kDim);
    for (int c = 0; c < totalCols; ++c) {
        const int x = colX(c, w, numCols);
        const int cw = colW(c, w, numCols);
        if (c == 0) {
            g.drawText("#", x, 0, cw, kHeaderH, juce::Justification::centred);
        } else if (c == 1) {
            g.drawText("on", x, 0, cw, kHeaderH, juce::Justification::centred);
        } else {
            const int dataCol = c - 2;
            const auto& meta = sheet.getColumn(dataCol);
            g.drawText(arp::columnLetters(dataCol), x, 2, cw, 12, juce::Justification::centred);
            g.setFont(mono(9.0f));
            g.drawText(meta.name, x, 14, cw, 14, juce::Justification::centred);
            g.setFont(mono(10.0f, true));
        }
    }

    const int active = numActiveSteps();
    const int playing = proc.getPlayingStep();
    const int vis = visibleRows();

    for (int i = 0; i <= vis && scrollRow + i < arp::kMaxRows; ++i) {
        const int r = scrollRow + i;
        const int y = kHeaderH + i * kRowH;
        const bool inPattern = r < active;

        g.setColour(!inPattern ? kInactive : r == playing ? kPlay : (r % 4 == 0 ? kBeat : kBg));
        g.fillRect(0, y, w, kRowH);

        g.setFont(mono(13.0f));
        g.setColour(r % 4 == 0 && inPattern ? kText : kDim);
        g.drawText(juce::String(r).paddedLeft('0', 2), 6, y, kIndexW - 6, kRowH, juce::Justification::centredLeft);

        // On/off toggle.
        const bool stepOn = sheet.isStepActive(r);
        juce::Rectangle<int> onCell(colX(1, w, numCols), y, colW(1, w, numCols), kRowH);
        const bool onCursor = r == row && col == 1;
        if (onCursor) {
            g.setColour(hasKeyboardFocus(true) ? kCursor : kCursor.withAlpha(0.4f));
            g.fillRect(onCell.reduced(1));
        }
        g.setColour(stepOn ? kText : kDim);
        g.drawText(stepOn ? "x" : ".", onCell.reduced(6, 0), juce::Justification::centred);

        // Data columns.
        for (int c = 0; c < numCols; ++c) {
            const int x = colX(c + 2, w, numCols);
            const int cw = colW(c + 2, w, numCols);
            const auto& pv = preview[static_cast<size_t>(r)][static_cast<size_t>(c)];
            juce::Rectangle<int> cell(x, y, cw, kRowH);

            const bool cursor = r == row && col == c + 2;
            if (cursor) {
                g.setColour(hasKeyboardFocus(true) ? kCursor : kCursor.withAlpha(0.4f));
                g.fillRect(cell.reduced(1));
            }

            juce::Colour col = pv.isError ? kError : (pv.isDefault || !stepOn || !inPattern) ? kDim : kText;
            if (cursor)
                col = juce::Colours::black;
            g.setColour(col);
            g.drawText(pv.text + (pv.isRandom ? "~" : ""), cell.reduced(6, 0), juce::Justification::centredLeft);
        }

        g.setColour(kGridLine);
        g.drawHorizontalLine(y + kRowH - 1, 0.0f, static_cast<float>(w));
    }

    g.setColour(kGridLine);
    for (int c = 0; c < totalCols; ++c)
        g.drawVerticalLine(colX(c, w, numCols), 0.0f, static_cast<float>(getHeight()));
}

// ---------------------------------------------------------------------------
// Input

void TrackerGrid::select(int newRow, int newCol)
{
    const int numCols = proc.getSheet().getNumColumns();
    const int totalCols = numCols + 2;
    row = juce::jlimit(0, arp::kMaxRows - 1, newRow);
    col = juce::jlimit(0, totalCols - 1, newCol);
    ensureVisible();
    repaint();
    if (onSelectionChanged)
        onSelectionChanged();
}

bool TrackerGrid::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto ch = key.getTextCharacter();
    const int code = key.getKeyCode();
    const bool wasG = std::exchange(pendingG, false);

    auto requestEdit = [this](const juce::String& initial) {
        if (col == 1)
            toggleStep(row);
        else if (col >= 2 && onEditRequested)
            onEditRequested(initial);
    };

    // Undo/redo: vim and platform shortcuts.
    if ((mods.isCommandDown() && code == 'Z' && mods.isShiftDown()) || (mods.isCtrlDown() && code == 'R')) {
        redo();
        return true;
    }
    if (mods.isCommandDown() && code == 'Z') {
        undo();
        return true;
    }
    if (mods.isCtrlDown() && (code == 'D' || code == 'U')) {
        select(row + (code == 'D' ? 1 : -1) * visibleRows() / 2, col);
        return true;
    }
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
        return false; // leave other shortcuts to the host

    if (key == juce::KeyPress::upKey || ch == 'k') { select(row - 1, col); return true; }
    if (key == juce::KeyPress::downKey || ch == 'j') { select(row + 1, col); return true; }
    if (key == juce::KeyPress::leftKey || ch == 'h') { select(row, col - 1); return true; }
    if (key == juce::KeyPress::rightKey || ch == 'l') { select(row, col + 1); return true; }
    if (key == juce::KeyPress::tabKey) { select(row, col + (mods.isShiftDown() ? -1 : 1)); return true; }
    if (key == juce::KeyPress::pageUpKey) { select(row - visibleRows(), col); return true; }
    if (key == juce::KeyPress::pageDownKey) { select(row + visibleRows(), col); return true; }
    if (key == juce::KeyPress::homeKey) { select(0, col); return true; }
    if (key == juce::KeyPress::endKey || ch == 'G') { select(numActiveSteps() - 1, col); return true; }
    if (ch == 'g') {
        if (wasG)
            select(0, col);
        else
            pendingG = true;
        return true;
    }

    if (ch == ' ') { toggleStep(row); return true; }
    if (ch == 'u') { undo(); return true; }
    if (ch == 'y') { clipboard = cellFormula(); return true; }
    if (ch == 'p') { setCellFormula(clipboard); return true; }
    if (ch == 'x' || key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        if (col == 1)
            toggleStep(row);
        else if (col >= 2)
            setCellFormula({});
        return true;
    }
    if (ch == 'i' || ch == 'a' || key == juce::KeyPress::returnKey || key == juce::KeyPress::F2Key) {
        requestEdit(cellFormula());
        return true;
    }
    if (ch == '=' || ch == '-' || ch == '.' || juce::CharacterFunctions::isDigit(ch)) {
        requestEdit(juce::String::charToString(ch)); // spreadsheet-style: typing replaces
        return true;
    }
    return false;
}

void TrackerGrid::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    int r, c;
    if (!cellAt(e.getPosition(), r, c))
        return;
    select(r, c);
    if (c == 1 && e.getNumberOfClicks() == 1)
        toggleStep(r);
}

void TrackerGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    int r, c;
    if (cellAt(e.getPosition(), r, c) && c >= 2 && onEditRequested)
        onEditRequested(cellFormula());
}

void TrackerGrid::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int vis = visibleRows();
    scrollRow = juce::jlimit(0, juce::jmax(0, arp::kMaxRows - vis),
                             scrollRow - juce::roundToInt(wheel.deltaY * 12.0f));
    repaint();
}
