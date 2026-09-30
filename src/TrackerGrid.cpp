#include "TrackerGrid.h"

namespace {

constexpr int kRowH = 20;
constexpr int kHeaderH = 22;
constexpr int kIndexW = 34;
constexpr int kOnW = 32;
constexpr size_t kMaxUndo = 200;
const char* const kLaneNames[] = {"on", "note", "vel", "gate", "len"};

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

TrackerGrid::TrackerGrid(ArpExcelAudioProcessor& p) : proc(p)
{
    setWantsKeyboardFocus(true);
    refresh();
    startTimerHz(30);
}

// ---------------------------------------------------------------------------
// Model access

juce::String* TrackerGrid::formulaRef(int r, int l)
{
    auto& step = proc.getPattern().getStep(r);
    switch (l) {
    case Note: return &step.noteFormula;
    case Velocity: return &step.velocityFormula;
    case Gate: return &step.gateFormula;
    case Length: return &step.lengthFormula;
    default: return nullptr;
    }
}

juce::String TrackerGrid::cellName() const
{
    return juce::String(row) + " " + kLaneNames[lane];
}

juce::String TrackerGrid::cellFormula() const
{
    auto* self = const_cast<TrackerGrid*>(this);
    if (auto* f = self->formulaRef(row, lane))
        return *f;
    return proc.getPattern().getStep(row).active ? "on" : "off";
}

juce::String TrackerGrid::cellError() const
{
    auto* self = const_cast<TrackerGrid*>(this);
    if (auto* f = self->formulaRef(row, lane); f != nullptr && f->trim().isNotEmpty()) {
        std::string err;
        arp::formula::Program::compile(f->toStdString(), err);
        return juce::String(err);
    }
    return {};
}

void TrackerGrid::edit(const std::function<void()>& change)
{
    undoStack.push_back(proc.getPattern());
    if (undoStack.size() > kMaxUndo)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    change();
    proc.patternChanged(); // bumps the version; the timer refreshes the preview
    refresh();
}

void TrackerGrid::setCellFormula(const juce::String& text)
{
    if (lane == On) {
        auto t = text.trim().toLowerCase();
        const bool on = !(t == "0" || t == "off" || t == "." || t == "false");
        if (proc.getPattern().getStep(row).active != on)
            toggleStep(row);
        return;
    }
    auto* f = formulaRef(row, lane);
    if (f == nullptr || *f == text.trim())
        return;
    edit([f, t = text.trim()] { *f = t; });
}

void TrackerGrid::toggleStep(int r)
{
    edit([this, r] {
        auto& s = proc.getPattern().getStep(r);
        s.active = !s.active;
    });
}

void TrackerGrid::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back(proc.getPattern());
    proc.getPattern() = undoStack.back();
    undoStack.pop_back();
    proc.patternChanged();
    refresh();
}

void TrackerGrid::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back(proc.getPattern());
    proc.getPattern() = redoStack.back();
    redoStack.pop_back();
    proc.patternChanged();
    refresh();
}

// ---------------------------------------------------------------------------
// Preview

void TrackerGrid::refresh()
{
    seenVersion = proc.getPatternVersion();

    juce::StringArray ignored;
    const auto compiled = proc.getPattern().compile(ignored);
    const double defaultGate = proc.getAPVTS().getRawParameterValue("gate")->load();

    uint32_t rng = 0x9e3779b9u;
    int prev = 0;
    for (int r = 0; r < arp::kMaxSteps; ++r) {
        const auto& src = proc.getPattern().getStep(r);
        auto& out = preview[static_cast<size_t>(r)];
        out[On] = {src.active ? "x" : ".", false, false, false};

        const int input = kPreviewChord[r % 3];
        const auto res = arp::evaluateStep(*compiled, r, {1, input, 100}, prev, defaultGate, &rng);
        if (res.playable && src.active)
            prev = res.pitch;

        const juce::String* formulas[] = {nullptr, &src.noteFormula, &src.velocityFormula, &src.gateFormula,
                                          &src.lengthFormula};
        for (int l = Note; l < NumLanes; ++l) {
            Preview pv;
            const auto& text = *formulas[l];
            pv.isDefault = text.trim().isEmpty();
            pv.isRandom = text.containsIgnoreCase("RANDOM");
            if (!pv.isDefault) {
                std::string err;
                arp::formula::Program::compile(text.toStdString(), err);
                pv.isError = !err.empty();
            }
            if (pv.isError) {
                pv.text = "ERR";
            } else {
                switch (l) {
                case Note:
                    pv.text = !src.active ? "---" // rest
                            : res.playable ? juce::MidiMessage::getMidiNoteName(res.pitch, true, true, 4) : "--";
                    break;
                case Velocity: pv.text = juce::String(res.velocity); break;
                case Gate: pv.text = juce::String(juce::roundToInt(res.gate * 100)) + "%"; break;
                case Length: pv.text = juce::String(res.length, res.length == std::floor(res.length) ? 0 : 2); break;
                default: break;
                }
            }
            out[static_cast<size_t>(l)] = pv;
        }
    }
    repaint();
    if (onSelectionChanged)
        onSelectionChanged();
}

void TrackerGrid::timerCallback()
{
    if (proc.getPatternVersion() != seenVersion)
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
    scrollRow = juce::jlimit(0, juce::jmax(0, arp::kMaxSteps - vis), scrollRow);
}

static int laneX(int lane, int width)
{
    if (lane == 0)
        return kIndexW;
    const int w = (width - kIndexW - kOnW) / 4;
    return kIndexW + kOnW + (lane - 1) * w;
}

static int laneW(int lane, int width)
{
    return lane == 0 ? kOnW : (width - kIndexW - kOnW) / 4;
}

bool TrackerGrid::cellAt(juce::Point<int> pt, int& r, int& l) const
{
    if (pt.y < kHeaderH || pt.x < kIndexW)
        return false;
    r = scrollRow + (pt.y - kHeaderH) / kRowH;
    if (r >= arp::kMaxSteps)
        return false;
    for (l = NumLanes - 1; l > 0 && pt.x < laneX(l, getWidth()); --l) {}
    return true;
}

void TrackerGrid::paint(juce::Graphics& g)
{
    const int w = getWidth();
    g.fillAll(kBg);

    g.setFont(mono(12.0f, true));
    g.setColour(kDim);
    for (int l = 0; l < NumLanes; ++l)
        g.drawText(kLaneNames[l], laneX(l, w), 0, laneW(l, w), kHeaderH, juce::Justification::centred);

    const int active = numActiveSteps();
    const int playing = proc.getPlayingStep();
    const int vis = visibleRows();

    for (int i = 0; i <= vis && scrollRow + i < arp::kMaxSteps; ++i) {
        const int r = scrollRow + i;
        const int y = kHeaderH + i * kRowH;
        const bool inPattern = r < active;

        g.setColour(!inPattern ? kInactive : r == playing ? kPlay : (r % 4 == 0 ? kBeat : kBg));
        g.fillRect(0, y, w, kRowH);

        g.setFont(mono(13.0f));
        g.setColour(r % 4 == 0 && inPattern ? kText : kDim);
        g.drawText(juce::String(r).paddedLeft('0', 2), 6, y, kIndexW - 6, kRowH, juce::Justification::centredLeft);

        const bool stepOn = proc.getPattern().getStep(r).active;
        for (int l = 0; l < NumLanes; ++l) {
            const auto& pv = preview[static_cast<size_t>(r)][static_cast<size_t>(l)];
            juce::Rectangle<int> cell(laneX(l, w), y, laneW(l, w), kRowH);

            const bool cursor = r == row && l == lane;
            if (cursor) {
                g.setColour(hasKeyboardFocus(true) ? kCursor : kCursor.withAlpha(0.4f));
                g.fillRect(cell.reduced(1));
            }

            juce::Colour c = pv.isError ? kError : (pv.isDefault || !stepOn || !inPattern) ? kDim : kText;
            if (cursor)
                c = juce::Colours::black;
            g.setColour(c);
            g.drawText(pv.text + (pv.isRandom ? "~" : ""), cell.reduced(6, 0),
                       l == On ? juce::Justification::centred : juce::Justification::centredLeft);
        }
        g.setColour(kGridLine);
        g.drawHorizontalLine(y + kRowH - 1, 0.0f, static_cast<float>(w));
    }

    g.setColour(kGridLine);
    for (int l = 0; l < NumLanes; ++l)
        g.drawVerticalLine(laneX(l, w), 0.0f, static_cast<float>(getHeight()));
}

// ---------------------------------------------------------------------------
// Input

void TrackerGrid::select(int newRow, int newLane)
{
    row = juce::jlimit(0, arp::kMaxSteps - 1, newRow);
    lane = juce::jlimit(0, NumLanes - 1, newLane);
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
        if (lane == On)
            toggleStep(row);
        else if (onEditRequested)
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
        select(row + (code == 'D' ? 1 : -1) * visibleRows() / 2, lane);
        return true;
    }
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
        return false; // leave other shortcuts to the host

    if (key == juce::KeyPress::upKey || ch == 'k') { select(row - 1, lane); return true; }
    if (key == juce::KeyPress::downKey || ch == 'j') { select(row + 1, lane); return true; }
    if (key == juce::KeyPress::leftKey || ch == 'h') { select(row, lane - 1); return true; }
    if (key == juce::KeyPress::rightKey || ch == 'l') { select(row, lane + 1); return true; }
    if (key == juce::KeyPress::tabKey) { select(row, lane + (mods.isShiftDown() ? -1 : 1)); return true; }
    if (key == juce::KeyPress::pageUpKey) { select(row - visibleRows(), lane); return true; }
    if (key == juce::KeyPress::pageDownKey) { select(row + visibleRows(), lane); return true; }
    if (key == juce::KeyPress::homeKey) { select(0, lane); return true; }
    if (key == juce::KeyPress::endKey || ch == 'G') { select(numActiveSteps() - 1, lane); return true; }
    if (ch == 'g') {
        if (wasG)
            select(0, lane);
        else
            pendingG = true;
        return true;
    }

    if (ch == ' ') { toggleStep(row); return true; }
    if (ch == 'u') { undo(); return true; }
    if (ch == 'y') { clipboard = cellFormula(); return true; }
    if (ch == 'p') { setCellFormula(clipboard); return true; }
    if (ch == 'x' || key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        if (lane == On)
            toggleStep(row);
        else
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
    int r, l;
    if (!cellAt(e.getPosition(), r, l))
        return;
    select(r, l);
    if (l == On && e.getNumberOfClicks() == 1)
        toggleStep(r);
}

void TrackerGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    int r, l;
    if (cellAt(e.getPosition(), r, l) && l != On && onEditRequested)
        onEditRequested(cellFormula());
}

void TrackerGrid::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int vis = visibleRows();
    scrollRow = juce::jlimit(0, juce::jmax(0, arp::kMaxSteps - vis),
                             scrollRow - juce::roundToInt(wheel.deltaY * 12.0f));
    repaint();
}
