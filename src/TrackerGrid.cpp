#include "TrackerGrid.h"

namespace {

constexpr size_t kMaxUndo = 200;

const juce::Colour kBg(0xff16181c), kBeat(0xff1d2026), kGridLine(0xff262a31), kText(0xffd6dbe2),
    kDim(0xff646c78), kCursor(0xffe0b050), kPlay(0xff2c4a3a), kError(0xffff7a59), kInactive(0xff101114),
    kSel(0xff3a5a8c80);

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
    const auto [r0, r1] = selRows();
    const auto [c0, c1] = selCols();
    if (r0 == r1 && c0 == c1 && c0 >= 1)
        return juce::String(arp::columnLetters(c0 - 1)) + juce::String(r0 + 1);
    return juce::String(arp::columnLetters(c0 - 1)) + juce::String(r0 + 1) + ":" +
           juce::String(arp::columnLetters(juce::jmax(0, c1 - 1))) + juce::String(r1 + 1);
}

std::pair<int, int> TrackerGrid::selRows() const
{
    return { juce::jmin(anchorRow, selRowEnd), juce::jmax(anchorRow, selRowEnd) };
}

std::pair<int, int> TrackerGrid::selCols() const
{
    return { juce::jmin(anchorCol, selColEnd), juce::jmax(anchorCol, selColEnd) };
}

bool TrackerGrid::isActiveCell() const
{
    return anchorRow == selRowEnd && anchorCol == selColEnd;
}

juce::String TrackerGrid::cellFormula() const
{
    const int dCol = col - 1;
    auto* self = const_cast<TrackerGrid*>(this);
    if (dCol >= 0 && dCol < proc.getSheet().getNumColumns())
        if (auto* f = self->formulaRef(row, dCol))
            return juce::String(*f);
    return proc.getSheet().isStepActive(row) ? "on" : "off";
}

juce::String TrackerGrid::cellError() const
{
    const int dCol = col - 1;
    auto* self = const_cast<TrackerGrid*>(this);
    if (dCol >= 0 && dCol < proc.getSheet().getNumColumns())
        if (auto* f = self->formulaRef(row, dCol); f != nullptr && !juce::String(*f).trim().isEmpty()) {
            std::string err;
            const auto names = proc.getSheet().getColumnNames();
            arp::formula::Program::compile(*f, err, proc.getSheet().getNumColumns(), &names);
            // A note name in a Note column ("C3", "=F#4") is valid input; the
            // sheet stores it as that note rather than a reference to cell C3.
            if (!err.empty() && proc.getSheet().getColumn(dCol).type == arp::ColumnType::Note) {
                auto t = juce::String(*f).trim();
                if (t.startsWith("="))
                    t = t.substring(1).trim();
                if (arp::formula::parseNoteName(t.toStdString()) >= 0.0)
                    return {};
            }
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
    const auto rows = selRows();
    const auto cols = selCols();
    const auto t = text.trim().toStdString();
    edit([&] {
        auto& s = proc.getSheet();
        for (int r = rows.first; r <= rows.second; ++r) {
            for (int c = cols.first; c <= cols.second; ++c) {
                const int dCol = c - 1;
                if (dCol < 0 || dCol >= s.getNumColumns() || r < 0 || r >= arp::kMaxRows)
                    continue;
                if (t.empty())
                    s.clearCell(dCol, r);
                else
                    s.setCellFormula(dCol, r, t);
            }
        }
    });
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

void TrackerGrid::select(int newRow, int newCol, bool extend)
{
    const int numCols = proc.getSheet().getNumColumns();
    const int totalCols = numCols + 1;
    row = juce::jlimit(0, proc.getSheet().getNumRows() - 1, newRow);
    col = juce::jlimit(1, totalCols - 1, newCol);
    if (!extend) {
        anchorRow = row;
        anchorCol = col;
        selRowEnd = row;
        selColEnd = col;
    } else {
        selRowEnd = row;
        selColEnd = col;
    }
    ensureVisible();
    repaint();
    if (onSelectionChanged)
        onSelectionChanged();
}

void TrackerGrid::jumpTo(const juce::String& ref)
{
    auto t = ref.trim().upToFirstOccurrenceOf(":", false, false);
    t = t.trim().toUpperCase();
    t = t.replaceCharacter('$', ' ').trim();
    int split = 0;
    while (split < t.length() && t[split] >= 'A' && t[split] <= 'Z')
        ++split;
    if (split == 0 || split == t.length())
        return;
    const int c = arp::columnIndex(t.substring(0, split).toStdString());
    const int r = t.substring(split).getIntValue() - 1;
    const int numCols = proc.getSheet().getNumColumns();
    if (c < 0 || c >= numCols || r < 0 || r >= proc.getSheet().getNumRows())
        return;
    select(r, c + 1);
}

// ---------------------------------------------------------------------------
// Range operations

void TrackerGrid::copySelection()
{
    const auto [r0, r1] = selRows();
    const auto [c0, c1] = selCols();
    auto& sheet = proc.getSheet();
    juce::String out;
    for (int r = r0; r <= r1; ++r) {
        if (r > r0)
            out += '\n';
        for (int c = c0; c <= c1; ++c) {
            if (c > c0)
                out += '\t';
            const int dCol = c - 1;
            const auto& f = sheet.getCellFormula(dCol, r);
            if (!f.empty())
                out += juce::String(f);
            else if (sheet.hasCellValue(dCol, r))
                out += juce::String(sheet.getCellValue(dCol, r),
                                    sheet.getCellValue(dCol, r) == std::floor(sheet.getCellValue(dCol, r)) ? 0 : 2);
        }
    }
    clipboard = out;
    juce::SystemClipboard::copyTextToClipboard(out);
}

void TrackerGrid::pasteIntoSelection()
{
    juce::String text = juce::SystemClipboard::getTextFromClipboard().trimEnd();
    if (text.isEmpty())
        text = clipboard;
    if (text.isEmpty())
        return;
    const auto& sheet = proc.getSheet();
    const int numRows = arp::kMaxRows;
    const int numCols = sheet.getNumColumns();
    auto lines = juce::StringArray::fromLines(text);
    auto isValue = [](const juce::String& t) {
        const auto s = t.trim();
        if (s.isEmpty())
            return false;
        bool hasDigit = false;
        for (auto c : s)
            if (juce::CharacterFunctions::isDigit(c))
                hasDigit = true;
        return hasDigit && s.containsOnly("0123456789.+-eE,");
    };
    edit([&] {
        auto& s = proc.getSheet();
        for (int i = 0; i < lines.size(); ++i) {
            auto cells = juce::StringArray::fromTokens(lines[i], "\t", "");
            for (int j = 0; j < cells.size(); ++j) {
                const int r = row + i;
                const int dCol = (col - 1) + j;
                if (r < 0 || r >= numRows || dCol < 0 || dCol >= numCols)
                    continue;
                const auto t = cells[j].trim();
                if (pasteMode_ == 1 && !t.startsWith("="))
                    continue;
                if (pasteMode_ == 2 && !isValue(t))
                    continue;
                if (t.isEmpty())
                    s.clearCell(dCol, r);
                else
                    s.setCellFormula(dCol, r, t.toStdString());
            }
        }
    });
}

void TrackerGrid::copyRowText(int r)
{
    auto& sheet = proc.getSheet();
    juce::String out;
    for (int c = 0; c < sheet.getNumColumns(); ++c) {
        if (c > 0)
            out << '\t';
        const auto f = sheet.getCellFormula(c, r);
        if (!f.empty())
            out << juce::String(f);
        else if (sheet.hasCellValue(c, r))
            out << juce::String(sheet.getCellValue(c, r),
                                sheet.getCellValue(c, r) == std::floor(sheet.getCellValue(c, r)) ? 0 : 2);
    }
    clipboard = out;
    juce::SystemClipboard::copyTextToClipboard(out);
}

void TrackerGrid::pasteRowText(int r)
{
    juce::String text = juce::SystemClipboard::getTextFromClipboard().trimEnd();
    if (text.isEmpty())
        return;
    auto lines = juce::StringArray::fromLines(text);
    edit([&] {
        auto& s = proc.getSheet();
        for (int i = 0; i < lines.size(); ++i) {
            auto cells = juce::StringArray::fromTokens(lines[i], "\t", "");
            for (int j = 0; j < cells.size(); ++j) {
                if (r + i < 0 || r + i >= s.getNumRows() || j < 0 || j >= s.getNumColumns())
                    continue;
                const auto t = cells[j].trim();
                if (t.isEmpty())
                    s.clearCell(j, r + i);
                else
                    s.setCellFormula(j, r + i, t.toStdString());
            }
        }
    });
}

void TrackerGrid::clearSelection()
{
    const auto rows = selRows();
    const auto cols = selCols();
    const int r0 = rows.first, r1 = rows.second, c0 = cols.first, c1 = cols.second;
    const bool range = !(r0 == r1 && c0 == c1);
    edit([&] {
        auto& s = proc.getSheet();
        if (range) {
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c)
                    s.clearCell(c - 1, r);
        } else {
            s.clearCell(col - 1, row);
        }
    });
}

// ---------------------------------------------------------------------------
// Preview

void TrackerGrid::refresh()
{
    seenVersion = proc.getSheetVersion();

    auto& sheet = proc.getSheet();
    const int numCols = sheet.getNumColumns();
    const auto names = sheet.getColumnNames();

    juce::StringArray ignored;
    const auto compiled = sheet.compile(ignored);
    const double defaultGate = proc.getAPVTS().getRawParameterValue("gate")->load();

    // Resize preview to [row][col].
    preview.assign(static_cast<size_t>(arp::kMaxRows), std::vector<Preview>(static_cast<size_t>(numCols)));

    uint32_t rng = 0x9e3779b9u;
    int prev = 0;
    for (int r = 0; r < arp::kMaxRows; ++r) {
        const int input = kPreviewChord[r % 3];
        const auto res = arp::evaluateStep(*compiled, r, {1, input, 100}, prev, defaultGate, &rng, 1.0);
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
                arp::formula::Program::compile(text.toStdString(), err, numCols, &names);
                if (!err.empty() && meta.type == arp::ColumnType::Note) {
                    auto t = text.trim();
                    if (t.startsWith("="))
                        t = t.substring(1).trim();
                    if (arp::formula::parseNoteName(t.toStdString()) >= 0.0)
                        err.clear();
                }
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
                        pv.text = val < 0 ? "---" : juce::String(juce::roundToInt(val));
                        break;
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

int TrackerGrid::visibleRows() const
{
    return juce::jmax(1, (getHeight() - headerH()) / rowH());
}

void TrackerGrid::autoFitColumnWidths()
{
    auto& sheet = proc.getSheet();
    customColWidths.assign(static_cast<size_t>(sheet.getNumColumns()), 72);
    for (int r = 0; r < arp::kMaxRows; ++r) {
        for (int c = 0; c < static_cast<int>(preview[r].size()); ++c) {
            const int want = static_cast<int>(preview[r][c].text.length()) * 9 + 16;
            customColWidths[static_cast<size_t>(c)] =
                juce::jmax(28, juce::jmin(240, juce::jmax(customColWidths[c], want)));
        }
    }
    repaint();
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
    scrollRow = juce::jlimit(0, juce::jmax(0, proc.getSheet().getNumRows() - vis), scrollRow);
}

// Column grid: strip is the row-number strip; data column widths come from
// loopStart custom widths (autofit) scaled by zoom, otherwise a padded zoom
// of the even split.
int TrackerGrid::colX(int c, int width, int numCols) const
{
    if (c <= 1)
        return stripW();
    int x = stripW();
    for (int i = 1; i < c; ++i)
        x += colW(i, width, numCols);
    return x;
}

int TrackerGrid::colW(int c, int width, int numCols) const
{
    if (c == 0)
        return stripW();
    const int idx = c - 1;
    if (!customColWidths.empty() && idx >= 0 && idx < static_cast<int>(customColWidths.size()))
        return std::max(16, static_cast<int>(customColWidths[static_cast<size_t>(idx)] * zoom));
    const int even = (width - stripW()) / std::max(1, numCols);
    return std::max(16, static_cast<int>(even * zoom));
}

bool TrackerGrid::cellAt(juce::Point<float> pt, int& r, int& c) const
{
    if (pt.y < headerH() || pt.x < stripW())
        return false;
    r = scrollRow + static_cast<int>(pt.y - headerH()) / rowH();
    if (r >= proc.getSheet().getNumRows())
        return false;
    const int numCols = proc.getSheet().getNumColumns();
    const int totalCols = numCols + 1; // index + data
    for (c = totalCols - 1; c > 0 && pt.x < colX(c, getWidth(), numCols); --c) {}
    return true; // c in 1..numCols: grid col; dataCol = c - 1
}

void TrackerGrid::headerAt(juce::Point<float> pt, int& c, bool& isOnColumn) const
{
    c = -1;
    isOnColumn = false;
    if (pt.y >= headerH() || pt.x < stripW())
        return;
    const int numCols = proc.getSheet().getNumColumns();
    const int totalCols = numCols + 1;
    for (c = totalCols - 1; c > 0 && pt.x < colX(c, getWidth(), numCols); --c) {}
    if (c >= 1)
        c -= 1; // data column
}

int TrackerGrid::numActiveSteps() const
{
    return proc.getSheet().getNumRows();
}

void TrackerGrid::paint(juce::Graphics& g)
{
    const int w = getWidth();
    auto& sheet = proc.getSheet();
    const int numCols = sheet.getNumColumns();
    const int totalCols = numCols + 1; // index + on + data

    g.fillAll(kBg);

    // --- Header row ---
    // Select-all square.
    if (showRowNumbers) {
        g.setColour(juce::Colour(0xff2a2f38));
        g.fillRect(0, 0, stripW(), headerH());
        g.setColour(kDim);
        g.setFont(mono(10.0f, true));
        g.drawText("ALL", 0, 0, stripW(), headerH(), juce::Justification::centred);
    }

    if (showColumnHeaders)
    for (int c = 1; c < totalCols; ++c) {
        const int x = colX(c, w, numCols);
        const int cw = colW(c, w, numCols);
        const int dataCol = c - 1;
        const auto& meta = sheet.getColumn(dataCol);
        const bool hl = dataCol + 1 >= selCols().first && dataCol + 1 <= selCols().second &&
                        selRows().first == 0 && selRows().second == proc.getSheet().getNumRows() - 1;
        if (hl) {
            g.setColour(juce::Colour(0xff3a3f4a));
            g.fillRect(x, 0, cw, headerH());
        }
        g.setColour(meta.visible ? kDim : juce::Colour(0xff50545c));
        g.setFont(mono(10.0f, true));
        g.drawText(meta.name, x, 2, cw, 14, juce::Justification::centred);
        g.setFont(mono(9.0f));
        g.drawText(arp::columnLetters(dataCol), x, 14, cw, 14, juce::Justification::centred);
    }

    const int active = numActiveSteps();
    const int playing = proc.getPlayingStep();
    const int vis = visibleRows();
    const auto [sr0, sr1] = selRows();
    const auto [sc0, sc1] = selCols();

    for (int i = 0; i <= vis && scrollRow + i < proc.getSheet().getNumRows(); ++i) {
        const int r = scrollRow + i;
        const int y = headerH() + i * rowH();
        const bool inPattern = r < active;
        const bool rowHl = r >= sr0 && r <= sr1 && sc0 == 1 && sc1 == totalCols - 1;
        const bool stepOn = sheet.isStepActive(r);

        g.setColour(!inPattern ? kInactive : r == playing ? kPlay : (r % 4 == 0 ? kBeat : kBg));
        g.fillRect(0, y, w, rowH());

        // Hidden rows render as a thin stripe; their cells/audio are skipped.
        if (sheet.isRowHidden(r)) {
            g.setColour(juce::Colour(0xff101316));
            g.fillRect(0, y, w, rowH());
            g.setColour(juce::Colour(0xff4a4f58));
            g.drawText("hidden", 6, y, stripW() + 120, rowH(), juce::Justification::centredLeft);
            if (showGridlines) {
                g.setColour(kGridLine);
                g.drawHorizontalLine(y + rowH() - 1, 0.0f, static_cast<float>(w));
            }
            continue;
        }

        if (rowHl) {
            g.setColour(juce::Colour(0xff3a3f4a));
            g.fillRect(0, y, stripW(), rowH());
        }
        g.setColour(r % 4 == 0 && inPattern ? kText : kDim);
        if (!stepOn && r != playing)
            g.setColour(kDim.withAlpha(0.5f));
        if (r == playing)
            g.setColour(playheadColour);
        g.setFont(mono(13.0f));
        g.drawText(juce::String(r).paddedLeft('0', 2), 6, y, stripW() - 6, rowH(), juce::Justification::centredLeft);

        // Data columns.
        for (int c = 0; c < numCols; ++c) {
            const int x = colX(c + 1, w, numCols);
            const int cw = colW(c + 1, w, numCols);
        const auto& pv = preview[static_cast<size_t>(r)][static_cast<size_t>(c)];

            juce::Rectangle<int> cell(x, y, cw, rowH());
            const bool inSel = r >= sr0 && r <= sr1 && (c + 1) >= sc0 && (c + 1) <= sc1;

            if (inSel) {
                g.setColour(kSel);
                g.fillRect(cell.reduced(1));
            }

            const bool cursor = r == row && c + 1 == col;
            const bool rowActive = r == row || r == playing;
            juce::Colour colr = pv.isError ? kError
                                : (pv.isDefault || !stepOn || !inPattern) ? kDim : kText;
            if (!sheet.getColumn(c).visible && !pv.isDefault)
                colr = colr.withAlpha(0.5f);
            g.setColour(colr);
            g.setFont(mono(13.0f));
            g.drawText(pv.text + (pv.isRandom ? "~" : ""), cell.reduced(6, 0), juce::Justification::centredLeft);

            if (cursor) {
                g.setColour(hasKeyboardFocus(true) ? kCursor : kCursor.withAlpha(0.4f));
                // (Outline is drawn after the gridlines below so all four
                // sides survive the 1px-wide gutters.)
            } else if (rowActive && c + 1 >= sc0 && c + 1 <= sc1) {
                g.setColour(kCursor.withAlpha(0.12f));
                g.fillRect(cell.reduced(1));
            }
        }

        if (showGridlines) {
            g.setColour(kGridLine);
            g.drawHorizontalLine(y + rowH() - 1, 0.0f, static_cast<float>(w));
        }
    }

    // Column drag indicator: source header tinted, drop line at target.
    if (colDragActive) {
        const int fromX = colX(colDragFrom + 1, w, numCols);
        const int fromW = colW(colDragFrom + 1, w, numCols);
        g.setColour(juce::Colour(0xff3a5a8c));
        g.fillRect(fromX + 1, 0, fromW - 2, headerH());
        if (colDragOver >= 0) {
            const int x = colX(colDragOver + 1, w, numCols);
            g.fillRect(x - 1, 0, 3, getHeight());
        }
    }

    if (showGridlines) {
        g.setColour(kGridLine);
        for (int c = 0; c < totalCols; ++c)
            g.drawVerticalLine(colX(c, w, numCols), 0.0f, static_cast<float>(getHeight()));
    }

    // "Add row" stripe after the last row.
    {
        const int nr = sheet.getNumRows();
        const int ay = headerH() + (nr - scrollRow) * rowH();
        if (ay >= headerH() && ay < getHeight()) {
            g.setColour(juce::Colour(0xff22262e));
            g.fillRect(0, ay, w, rowH());
            g.setColour(kDim);
            g.drawText("+  Add row", 6, ay, stripW() + 120, rowH(), juce::Justification::centredLeft);
        }
    }

    // Active-cell outline, drawn after gridlines so none of its sides is
    // overpainted by the gutters.
    const int numColsNow = sheet.getNumColumns();
    if (row >= scrollRow && row < scrollRow + visibleRows() && col >= 1 && col <= numColsNow) {
        const int y = headerH() + (row - scrollRow) * rowH();
        const int x = colX(col, w, numColsNow);
        const int cw = colW(col, w, numColsNow);
        const juce::Rectangle<int> cell(x, y, cw, rowH());
        g.setColour(hasKeyboardFocus(true) ? kCursor : kCursor.withAlpha(0.4f));
        g.fillRect(cell.getX(), cell.getY(), cell.getWidth(), 1);
        g.fillRect(cell.getX(), cell.getBottom() - 1, cell.getWidth(), 1);
        g.fillRect(cell.getX(), cell.getY(), 1, cell.getHeight());
        g.fillRect(cell.getRight() - 1, cell.getY(), 1, cell.getHeight());
    }
}

// ---------------------------------------------------------------------------
// Input

bool TrackerGrid::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto ch = key.getTextCharacter();
    const int code = key.getKeyCode();
    const bool wasG = std::exchange(pendingG, false);
    const bool shift = mods.isShiftDown();

    auto requestEdit = [this](const juce::String& initial) {
        if (col >= 1 && onEditRequested)
            onEditRequested(initial);
    };

    if (mods.isCommandDown() && code == 'C') { copySelection(); return true; }
    if (mods.isCommandDown() && code == 'V') { pasteIntoSelection(); return true; }

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
        select(row + (code == 'D' ? 1 : -1) * visibleRows() / 2, col, shift);
        return true;
    }
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
        return false; // leave other shortcuts to the host

    if (key == juce::KeyPress::escapeKey) { select(row, col, false); return true; }
    if (key == juce::KeyPress::upKey || ch == 'k') { select(row - 1, col, shift); return true; }
    if (key == juce::KeyPress::downKey || ch == 'j') { select(row + 1, col, shift); return true; }
    if (key == juce::KeyPress::leftKey || ch == 'h') { select(row, col - 1, shift); return true; }
    if (key == juce::KeyPress::rightKey || ch == 'l') { select(row, col + 1, shift); return true; }
    if (key == juce::KeyPress::tabKey) { select(row, col + (shift ? -1 : 1), shift); return true; }
    if (key == juce::KeyPress::pageUpKey) { select(row - visibleRows(), col, shift); return true; }
    if (key == juce::KeyPress::pageDownKey) { select(row + visibleRows(), col, shift); return true; }
    if (key == juce::KeyPress::homeKey) { select(0, col, shift); return true; }
    if (key == juce::KeyPress::endKey || ch == 'G') { select(numActiveSteps() - 1, col, shift); return true; }
    if (ch == 'g') {
        if (wasG)
            select(0, col, shift);
        else
            pendingG = true;
        return true;
    }

    if (ch == ' ') { toggleStep(row); return true; }
    if (ch == 'u') { undo(); return true; }
    if (ch == 'y') { copySelection(); return true; }
    if (ch == 'p') { pasteIntoSelection(); return true; }
    if (ch == 'x' || key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        if (col >= 1)
            clearSelection();
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
    dragStart = e.position;
    colDragOver = -1;

    // --- Right click on a column header: context menu. ---
    if (e.mods.isRightButtonDown() && e.position.y < headerH()) {
        int c;
        bool isOnColumn;
        headerAt(e.position, c, isOnColumn);
        if (c >= 0 && !isOnColumn) {
            const auto& sheet = proc.getSheet();
            auto* m = new juce::PopupMenu();
            auto* typeSub = new juce::PopupMenu();
            for (int i = 0; i < 12; ++i)
                typeSub->addItem(100 + i, arp::columnTypeName(static_cast<arp::ColumnType>(i)));
            m->addItem(1, "Rename...");
            m->addSubMenu("Type", *typeSub);
            m->addItem(2, sheet.getColumn(c).visible ? "Hide column" : "Show column");
            m->addSeparator();
            m->addItem(3, "Add column to the left");
            m->addItem(4, "Add column to the right");
            m->addItem(5, "Remove column");
            auto screenPos = localPointToGlobal(e.position).toInt();
            auto options = juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1));
            m->showMenuAsync(options, juce::ModalCallbackFunction::create([this, m, c, typeSub](int result) {
                if (result == 1) {
                    auto* w = new juce::AlertWindow("Rename column", {}, juce::AlertWindow::NoIcon);
                    w->addTextEditor("name", proc.getSheet().getColumn(c).name, "Name");
                    w->addButton("OK", 1);
                    w->addButton("Cancel", 0);
                    w->enterModalState(true, juce::ModalCallbackFunction::create([this, w, c](int b) {
                        if (b == 1) {
                            auto name = w->getTextEditorContents("name").trim().toStdString();
                            if (!name.empty())
                                edit([this, c, name] { proc.getSheet().setColumnName(c, name); });
                        }
                        delete w;
                    }), false);
                } else if (result == 2) {
                    edit([this, c] {
                        auto& s = proc.getSheet();
                        s.setColumnVisible(c, !s.getColumn(c).visible);
                    });
                } else if (result >= 100 && result < 112) {
                    edit([this, c, result] {
                        auto& s = proc.getSheet();
                        s.setColumnType(c, static_cast<arp::ColumnType>(result - 100));
                    });
                } else if (result == 3) {
                    edit([this, c] {
                        auto& s = proc.getSheet();
                        const int added = s.addColumn(arp::ColumnType::Number, "Custom");
                        s.moveColumn(added, c);
                    });
                } else if (result == 4) {
                    edit([this, c] {
                        auto& s = proc.getSheet();
                        const int added = s.addColumn(arp::ColumnType::Number, "Custom");
                        s.moveColumn(added, c + 1);
                    });
                } else if (result == 5) {
                    edit([this, c] { proc.getSheet().removeColumn(c); });
                    const int total = proc.getSheet().getNumColumns() + 1;
                    col = juce::jlimit(1, total - 1, col);
                }
                delete typeSub;
                delete m;
            }));
            return;
        }
        return;
    }

    // --- Right click on the row-number strip: row context menu. ---
    if (e.mods.isRightButtonDown() && e.position.y >= headerH() && e.position.x < stripW()) {
        const int r = scrollRow + static_cast<int>(e.position.y - headerH()) / rowH();
        if (r < 0 || r >= proc.getSheet().getNumRows())
            return;
        auto& sheet = proc.getSheet();
        auto* m = new juce::PopupMenu();
        m->addItem(1, "Insert row below");
        m->addItem(2, "Insert row above");
        m->addItem(3, "Delete row");
        m->addSeparator();
        m->addItem(4, "Move row up", r > 0);
        m->addItem(5, "Move row down", r < sheet.getNumRows() - 1);
        m->addSeparator();
        m->addItem(6, sheet.isRowHidden(r) ? "Show row" : "Hide row");
        m->addSeparator();
        m->addItem(7, "Copy row");
        m->addItem(8, "Paste row");
        const auto screenPos = localPointToGlobal(e.position).toInt();
        auto options = juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
            juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1));
        m->showMenuAsync(options, juce::ModalCallbackFunction::create([this, m, r](int result) {
            auto doEdit = [this](const std::function<void()>& fn) { performEdit(fn); };
            switch (result) {
                case 1:
                    doEdit([this, r] { proc.getSheet().insertRow(r); });
                    selectCell(std::min(r + 1, proc.getSheet().getNumRows() - 1), col);
                    break;
                case 2:
                    doEdit([this, r] {
                        auto& s = proc.getSheet();
                        s.insertRow(r); // blank after r; move same pattern
                        s.moveRow(juce::jmin(r + 1, s.getNumRows() - 1), r);
                    });
                    selectCell(r, col);
                    break;
                case 3:
                    doEdit([this, r] { proc.getSheet().deleteRow(r); });
                    selectCell(juce::jmin(r, proc.getSheet().getNumRows() - 1), col);
                    break;
                case 4:
                    doEdit([this, r] { proc.getSheet().moveRow(r, r - 1); });
                    selectCell(r - 1, col);
                    break;
                case 5:
                    doEdit([this, r] { proc.getSheet().moveRow(r, r + 1); });
                    selectCell(r + 1, col);
                    break;
                case 6:
                    doEdit([this, r] { proc.getSheet().setRowHidden(r, !proc.getSheet().isRowHidden(r)); });
                    break;
                case 7: copyRowText(r); break;
                case 8: pasteRowText(r); break;
                default: break;
            }
            delete m;
        }));
        return;
    }

    // --- "+ Add row" stripe click. ---
    {
        const int nr = proc.getSheet().getNumRows();
        const int ay = headerH() + (nr - scrollRow) * rowH();
        if (e.position.y >= ay && e.position.y < ay + rowH() && e.position.x >= stripW()) {
            performEdit([this] { proc.getSheet().setNumRows(proc.getSheet().getNumRows() + 1); });
            return;
        }
    }

    // --- Clicks in the header band ---
    if (e.position.y < headerH()) {
        if (e.position.x < stripW()) {
            // Select-all square.
            select(0, 1, false);
            anchorCol = 1;
            selColEnd = proc.getSheet().getNumColumns();
            anchorRow = 0;
            selRowEnd = proc.getSheet().getNumRows() - 1;
            repaint();
            if (onSelectionChanged)
                onSelectionChanged();
            return;
        }
        int c;
        bool isOnColumn;
        headerAt(e.position, c, isOnColumn);
        if (isOnColumn)
            return;
        if (c >= 0) {
            colDragFrom = c; // a drag from here reorders columns
            const int gc = c + 1;
            if (e.mods.isShiftDown()) {
                anchorRow = 0;
                selRowEnd = proc.getSheet().getNumRows() - 1;
                selColEnd = gc;
            } else {
                anchorRow = 0;
                anchorCol = gc;
                selRowEnd = proc.getSheet().getNumRows() - 1;
                selColEnd = gc;
            }
            row = 0;
            col = gc;
            ensureVisible();
            repaint();
            if (onSelectionChanged)
                onSelectionChanged();
        }
        return;
    }

    // --- Click on a row number (index strip) ---
    if (e.position.y >= headerH() && e.position.x < stripW()) {
        const int r = scrollRow + static_cast<int>(e.position.y - headerH()) / rowH();
        if (r < 0 || r >= proc.getSheet().getNumRows())
            return;
        const int numCols = proc.getSheet().getNumColumns();
        if (e.mods.isShiftDown()) {
            anchorCol = 1;
            selColEnd = numCols;
            selRowEnd = r;
        } else {
            anchorCol = 1;
            anchorRow = r;
            selColEnd = numCols;
            selRowEnd = r;
        }
        row = r;
        col = 1;
        ensureVisible();
        repaint();
        if (onSelectionChanged)
            onSelectionChanged();
        return;
    }

    int r, c;
    if (!cellAt(e.position, r, c))
        return;

    colDragFrom = -1;
    select(r, c, e.mods.isShiftDown());
}

void TrackerGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
        return;

    // Column header drag: reorder.
    if (colDragFrom >= 0) {
        if (!colDragActive && (e.position - dragStart).getDistanceFromOrigin() > 4.0f)
            colDragActive = true;
        if (colDragActive) {
            int c;
            bool isOnColumn;
            headerAt(e.position, c, isOnColumn);
            colDragOver = c >= 0 && !isOnColumn ? c : -1;
            repaint();
            return;
        }
    }

    if (e.position.y < headerH() || e.position.x < stripW())
        return;
    int r, c;
    if (!cellAt(e.position, r, c))
        return;
    select(r, c, true);
}

void TrackerGrid::mouseUp(const juce::MouseEvent&)
{
    if (colDragActive && colDragOver >= 0 && colDragOver != colDragFrom) {
        const int from = colDragFrom;
        const int to = colDragOver;
        performEdit([this, from, to] {
            proc.getSheet().moveColumn(from, to);
            const int newCol = to + 1;
            if (col - 1 == from)
                col = newCol;
            else if (col >= 1) {
                const int d = col - 1;
                if (from < d && to >= d)
                    col = d; // shifted left by the moved column
                else if (from > d && to <= d)
                    col = d + 3; // shifted right onto d
            }
        });
    }
    colDragActive = false;
    colDragFrom = -1;
    colDragOver = -1;
    repaint();
}

void TrackerGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    int r, c;
    if (cellAt(e.position, r, c) && c >= 1 && onEditRequested)
        onEditRequested(cellFormula());
}

void TrackerGrid::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int vis = visibleRows();
    scrollRow = juce::jlimit(0, juce::jmax(0, proc.getSheet().getNumRows() - vis),
                             scrollRow - juce::roundToInt(wheel.deltaY * 12.0f));
    repaint();
}

juce::String TrackerGrid::activeCellValueText() const
{
    const int dCol = col - 1;
    if (dCol < 0 || dCol >= proc.getSheet().getNumColumns() || row < 0 || row >= proc.getSheet().getNumRows())
        return {};
    if (dCol < static_cast<int>(preview[static_cast<size_t>(row)].size()))
        return preview[row][dCol].text;
    return {};
}

void TrackerGrid::mouseMove(const juce::MouseEvent& e)
{
    const auto pt = e.position;
    const auto numCols = proc.getSheet().getNumColumns();
    auto publish = [this](const juce::String& tip) {
        if (currentHoverStatus != tip) {
            currentHoverStatus = tip;
            // Tooltips are intentionally not published here; the editor's
            // status bar already surfaces the same information.
            if (onHoverStatus)
                onHoverStatus(tip);
        }
    };

    // Header band: data type, column name, default value.
    if (pt.y < headerH() && pt.x >= stripW()) {
        int c;
        bool isOnColumn;
        headerAt(pt, c, isOnColumn);
        if (c >= 0 && c < numCols && !isOnColumn) {
            const auto& col = proc.getSheet().getColumn(c);
            publish(juce::String(col.name) + "  ·  type: " + arp::columnTypeName(col.type) +
                    "  ·  visible: " + (col.visible ? "yes" : "no") +
                    "  ·  default: " + juce::String(col.defaultValue, col.defaultValue == std::floor(col.defaultValue) ? 0 : 2));
            return;
        }
        publish({});
        return;
    }

    // Data cell: tooltip with formula and the evaluated value.
    int r, c;
    if (cellAt(pt, r, c) && c >= 1) {
        const int dCol = c - 1;
        const auto& col = proc.getSheet().getColumn(dCol);
        const auto& formula = proc.getSheet().getCellFormula(dCol, r);
        const auto value = (r >= 0 && r < proc.getSheet().getNumRows() && dCol >= 0 && dCol < numCols &&
                            dCol < static_cast<int>(preview[static_cast<size_t>(r)].size()))
                               ? preview[static_cast<size_t>(r)][static_cast<size_t>(dCol)].text
                               : juce::String {};
        publish(juce::String(arp::columnLetters(dCol)) + juce::String(r + 1) + "  ·  " +
                juce::String(col.name) + "  ·  " + arp::columnTypeName(col.type) +
                "  ·  value: " + (value.isEmpty() ? "(default)" : value) +
                (!formula.empty() ? "  ·  formula: " + juce::String(formula) : juce::String {}));
        return;
    }

    publish({});
}

void TrackerGrid::mouseExit(const juce::MouseEvent&)
{
    const auto tip = juce::String();
    if (currentHoverStatus != tip) {
        currentHoverStatus = tip;
        if (onHoverStatus)
            onHoverStatus({});
    }
}
