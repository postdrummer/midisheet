#include "TopPanel.h"

namespace {

juce::Font monoFont(float size = 14.0f)
{
    return juce::Font(juce::FontOptions("Iosevka Charon Mono", size, juce::Font::bold));
}

void style(juce::Component& c)
{
    if (auto* b = dynamic_cast<juce::Button*>(&c)) b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe6e9ee));
}

} // namespace

static double emptyComboValue(arp::ColumnType t)
{
    switch (t) {
        case arp::ColumnType::Note: return 1.0;
        case arp::ColumnType::Pitch: return -1.0;
        case arp::ColumnType::Length: return 1.0;
        case arp::ColumnType::Time: return 0.0;
        default: return 0.0;
    }
}
TopPanel::TopPanel(MidisheetAudioProcessor& p, TrackerGrid& g, juce::Label& info)
    : proc(p), grid(g), infoBar(info)
{
    fileTitle.setText("File", juce::dontSendNotification);
    defaultsTitle.setText("Defaults", juce::dontSendNotification);
    editTitle.setText("Edit", juce::dontSendNotification);
    for (auto* l : { &fileTitle, &defaultsTitle, &editTitle }) { l->setFont(monoFont(14.0f).boldened()); l->setColour(juce::Label::textColourId, juce::Colour(0xffe0b050)); addAndMakeVisible(l); }

    // ---- File ----
    restoreBtn.setButtonText("Restore Defaults");
    loadBtn.setButtonText("Load");
    saveBtn.setButtonText("Save");
    saveAsBtn.setButtonText("Save As...");
    restoreBtn.onClick = [this] {
        auto* w = new juce::AlertWindow("Restore Defaults",
                                        "Replace the current sheet with the default sheet? This cannot be undone.",
                                        juce::MessageBoxIconType::WarningIcon);
        w->addButton("OK", 1);
        w->addButton("Cancel", 0);
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int r) {
            juce::MessageManager::callAsync([this, w, r] {
                if (r == 1) {
                    grid.performEdit([this] { proc.getSheet().clear(); });
                    grid.repaint();
                    refreshFromSheet();
                }
                delete w;
            });
        }), false);
    };
    loadBtn.onClick = [this] { pickFile(false); };
    saveBtn.onClick = [this] { pickFile(true); };
    saveAsBtn.onClick = [this] { pickFile(true); };
    for (auto* b : { &restoreBtn, &loadBtn, &saveBtn, &saveAsBtn }) { b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2f38)); b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe6e9ee)); style(*b); addAndMakeVisible(b); }
    watchHover(restoreBtn, "Reset the sheet to the built-in default columns and content.");
    watchHover(loadBtn, "Load a sheet from a .json/.midisheet file.");
    watchHover(saveBtn, "Save the current sheet to a file.");
    watchHover(saveAsBtn, "Save the current sheet to a new file.");

    // ---- Defaults ----
    // (label, type, usesCombo). Numeric types get a text box; a few use
    // dropdowns with relevant choices, as requested.
    struct RowSpec { const char* label; arp::ColumnType type; bool combo; };
    const RowSpec specs[] = {
        { "Note",     arp::ColumnType::Note,     true  }, // boolean On/Off with empty=on
        { "Pitch",    arp::ColumnType::Pitch,    true  }, // combo of note names
        { "Chance",   arp::ColumnType::Chance,   false },
        { "Velocity", arp::ColumnType::Velocity, false },
        { "Gate",     arp::ColumnType::Gate,     false },
        { "Length",   arp::ColumnType::Length,   true  }, // combo of step lengths
        { "Shift",   arp::ColumnType::Shift,    false },
        { "Time",     arp::ColumnType::Time,     true  }, // combo of divisions
        { "Octave",   arp::ColumnType::Octave,   false },
        { "Percent",  arp::ColumnType::Percent,  false },
    };
    for (const auto& s : specs) {
        TypeRow row;
        row.type = s.type;
        row.label = s.label;
        row.labelComp = std::make_unique<juce::Label>(juce::String{}, row.label + ":");
        row.labelComp->setFont(monoFont());
        row.labelComp->setColour(juce::Label::textColourId, juce::Colour(0xff9aa3ad));
        addAndMakeVisible(*row.labelComp);
        if (s.combo) {
            row.combo = std::make_unique<DragCombo>();
            if (s.type == arp::ColumnType::Note) {
                row.combo->addItem("On", 1);
                row.combo->addItem("Off", 2);
                row.combo->onChange = [this, t = s.type, cb = row.combo.get()] {
                    const int idx = cb->getSelectedItemIndex();
                    applyDefault(t, idx == 1 ? 0.0 : 1.0);
                };
            } else if (s.type == arp::ColumnType::Pitch) {
                row.combo->addItem("Empty", 1);
                for (int n = 0; n < 128; ++n)
                    row.combo->addItem(juce::MidiMessage::getMidiNoteName(n, true, true, 4), n + 2);
                row.combo->onChange = [this, t = s.type, cb = row.combo.get()] {
                    const int idx = cb->getSelectedItemIndex();
                    applyDefault(t, idx <= 0 ? emptyComboValue(t) : (double)(idx - 1));
                };
            } else if (s.type == arp::ColumnType::Length) {
                row.combo->addItem("Empty", 1); // factory default (1 step)
                for (int i = 2; i <= 16; ++i) row.combo->addItem(juce::String(i) + " steps", i + 1);
                row.combo->onChange = [this, t = s.type, cb = row.combo.get()] {
                    const int idx = cb->getSelectedItemIndex();
                    applyDefault(t, idx <= 0 ? emptyComboValue(t) : (double)(idx + 1));
                };
            } else if (s.type == arp::ColumnType::Time) {
                row.combo->addItem("Empty", 1);
                const char* divs[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
                for (int i = 0; i < 6; ++i) row.combo->addItem(divs[i], i + 2);
                row.combo->onChange = [this, t = s.type, cb = row.combo.get()] {
                    const int idx = cb->getSelectedItemIndex();
                    applyDefault(t, idx <= 0 ? emptyComboValue(t) : (double) idx);
                };
            }
            addAndMakeVisible(*row.combo);
        } else {
            row.text = std::make_unique<juce::TextEditor>();
            row.text->setFont(monoFont());
            row.text->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e2127));
            row.text->setColour(juce::TextEditor::textColourId, juce::Colour(0xffe6e9ee));
            row.text->onReturnKey = [this, t = s.type, te = row.text.get()] {
                const auto text = te->getText().trim();
                if (text.isNotEmpty()) applyDefault(t, text.getDoubleValue());
            };
            addAndMakeVisible(*row.text);
        }
        defaultRows.push_back(std::move(row));
    }
    // (labels are created+added in buildDefaultsRow during layout via infos)
    for (auto& r : defaultRows) {
        watchHover(r.combo ? static_cast<juce::Component&>(*r.combo) : static_cast<juce::Component&>(*r.text),
                   "Default value used when a cell in this column type is empty.");
    }

    // ---- Edit ----
    addLabel_.setFont(monoFont());
    addLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff9aa3ad));
    addAndMakeVisible(addLabel_);
    addName.setText("1");
    addName.setFont(monoFont());
    addName.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e2127));
    addName.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe6e9ee));
    addType.addItemList(juce::StringArray{ "column", "row" }, 1);
    addType.setSelectedItemIndex(0, juce::dontSendNotification);
    addBtn.setButtonText("+");
    addBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2f38));
    addBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe6e9ee));
    addBtn.onClick = [this] {
        const int n = juce::jmax(1, addName.getText().getIntValue());
        const bool asRows = addType.getSelectedItemIndex() == 1;
        grid.performEdit([this, n, asRows] {
            auto& s = proc.getSheet();
            if (asRows)
                for (int i = 0; i < n; ++i) s.insertRow(grid.getRow());
            else {
                const int dc = grid.getCol() - 1;
                const auto type = (dc >= 0 && dc < s.getNumColumns()) ? s.getColumn(dc).type : arp::ColumnType::Number;
                for (int i = 0; i < n; ++i) s.addColumn(type, "New");
            }
        });
        refreshFromSheet();
    };
    deleteBtn.setButtonText("Delete");
    deleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2f38));
    deleteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe6e9ee));
    deleteBtn.onClick = [this] {
        const int dc = grid.getCol() - 1;
        if (dc < 0) return;
        grid.performEdit([this, dc] { proc.getSheet().deleteColumn(dc); });
        refreshFromSheet();
    };
    mergeBtn.setButtonText("Merge");
    mergeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2f38));
    mergeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe6e9ee));
    mergeBtn.onClick = [this] {
        const int dc = grid.getCol() - 1;
        if (dc < 0) return;
        const auto [r0, r1] = grid.selRows();
        if (r1 <= r0) return;
        grid.performEdit([this, dc, r0, r1] {
            auto& s = proc.getSheet();
            // Excel-style vertical merge: keep the top cell's content, blank the rest.
            juce::String keep;
            for (int r = r0; r <= r1; ++r)
                if (s.hasCellValue(dc, r) || juce::String(s.getCellFormula(dc, r)).trim().isNotEmpty()) { keep = s.getCellFormula(dc, r); break; }
            for (int r = r0; r <= r1; ++r) s.clearCell(dc, r);
            if (juce::String(keep).trim().isNotEmpty()) s.setCellFormula(dc, r0, keep.toStdString());
        });
        grid.repaint();
    };
    datatypeCombo.addItemList([] {
        juce::StringArray a;
        for (int i = 0; i <= static_cast<int>(arp::ColumnType::Formula); ++i)
            if (static_cast<arp::ColumnType>(i) != arp::ColumnType::CC)
                a.add(arp::columnTypeName(static_cast<arp::ColumnType>(i)));
        return a; }(), 1);
    dtLabel_.setFont(monoFont());
    dtLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff9aa3ad));
    addAndMakeVisible(dtLabel_);
    datatypeCombo.onChange = [this] {
        const int dc = grid.getCol() - 1;
        if (dc < 0) return;
        const int idx = datatypeCombo.getSelectedItemIndex();
        grid.performEdit([this, dc, idx] { proc.getSheet().setColumnType(dc, static_cast<arp::ColumnType>(idx)); });
    };
    for (auto* c : { static_cast<juce::Component*>(&addName), static_cast<juce::Component*>(&addType), static_cast<juce::Component*>(&addBtn),
                     static_cast<juce::Component*>(&deleteBtn), static_cast<juce::Component*>(&mergeBtn), static_cast<juce::Component*>(&datatypeCombo) }) { style(*c); addAndMakeVisible(c); }
    watchHover(addName, "How many to add.");
    watchHover(addType, "Add as columns or rows.");
    watchHover(addBtn, "Add the given number of columns (inheriting the active column's type) or rows after the active row.");
    watchHover(deleteBtn, "Delete the active column.");
    watchHover(mergeBtn, "Merge the selected cells vertically in the active column (keep top cell).");
    watchHover(datatypeCombo, "Change the data type of the highlighted column.");

    startTimerHz(5);
    refreshFromSheet();
}

TopPanel::~TopPanel() = default;

void TopPanel::pickFile(bool save)
{
    fileChooser_ = std::make_unique<juce::FileChooser>(save ? "Save sheet" : "Load sheet", juce::File{}, "*.json *.midisheet");
    const auto mode = save ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode;
    fileChooser_->launchAsync(mode, [this, save](const juce::FileChooser& chooser) {
        const auto f = chooser.getResult();
        if (f == juce::File{}) return;
        if (save) { f.replaceWithText(juce::JSON::toString(proc.getSheet().toVar(), true)); return; }
        if (juce::var v; juce::JSON::parse(f.loadFileAsString(), v).wasOk()) { proc.getSheet().fromVar(v); proc.sheetChanged(); refreshFromSheet(); }
    });
}



void TopPanel::applyDefault(arp::ColumnType type, double value)
{
    for (int c = 0; c < proc.getSheet().getNumColumns(); ++c)
        if (proc.getSheet().getColumn(c).type == type)
            proc.getSheet().setColumnDefault(c, value);
    proc.sheetChanged();
    grid.repaint();
}

void TopPanel::buildDefaultsRow(TypeRow& row, int x, int y, int w)
{
    // Layout helper used from resized(); labels handled there too.
    (void)row; (void)x; (void)y; (void)w;
}

void TopPanel::refreshDefaultsRow(TypeRow& row) const
{
    double v = 0.0; bool found = false;
    for (int c = 0; c < proc.getSheet().getNumColumns(); ++c)
        if (proc.getSheet().getColumn(c).type == row.type) { v = proc.getSheet().getColumn(c).defaultValue; found = true; break; }
    if (!found) return;
    if (row.combo) {
        int idx = 0;
        if (row.type == arp::ColumnType::Note)
            idx = (v < 0.5) ? 1 : 0;
        else if (row.type == arp::ColumnType::Pitch)
            idx = (v < 0) ? 0 : (int) v + 1;
        else if (row.type == arp::ColumnType::Length)
            idx = (v <= 1) ? 0 : (int) v - 1;
        else if (row.type == arp::ColumnType::Time)
            idx = (v <= 0) ? 0 : (int) v;
        idx = juce::jlimit(0, row.combo->getNumItems() - 1, idx);
        row.combo->setSelectedItemIndex(idx, juce::dontSendNotification);
        // Force the internal combo text label to re-derive its font so "Empty" can render italicised.
        row.combo->resized();

    } else {
        row.text->setText(juce::String(v, v == std::floor(v) ? 0 : 2), juce::dontSendNotification);
    }
}

void TopPanel::refreshFromSheet()
{
    for (auto& r : defaultRows) refreshDefaultsRow(r);
    const int dc = grid.getCol() - 1;
    if (dc >= 0 && dc < proc.getSheet().getNumColumns())
        datatypeCombo.setSelectedItemIndex(static_cast<int>(proc.getSheet().getColumn(dc).type), juce::dontSendNotification);
    seenSheetVersion = proc.getSheetVersion();
}

void TopPanel::timerCallback()
{
    if (proc.getSheetVersion() != seenSheetVersion) refreshFromSheet();
    // Track active-column type changes even without a version bump pathway.
    const int dc = grid.getCol() - 1;
    if (dc >= 0 && dc < proc.getSheet().getNumColumns()) {
        const int t = static_cast<int>(proc.getSheet().getColumn(dc).type);
        if (datatypeCombo.getSelectedItemIndex() != t && !datatypeCombo.isPopupActive())
            datatypeCombo.setSelectedItemIndex(t, juce::dontSendNotification);
    }
}

void TopPanel::setInfo(const juce::String& text) { infoBar.setText(text, juce::dontSendNotification); }

void TopPanel::watchHover(juce::Component& c, const juce::String& info)
{
    struct H : juce::MouseListener {
        juce::Label& bar; juce::String text;
        H(juce::Label& b, juce::String t) : bar(b), text(std::move(t)) {}
        void mouseEnter(const juce::MouseEvent&) override { bar.setText(text, juce::dontSendNotification); }
        void mouseExit(const juce::MouseEvent&) override { bar.setText({}, juce::dontSendNotification); }
    };
    auto h = std::make_unique<H>(infoBar, info);
    c.addMouseListener(h.get(), true);
    hoverListeners.push_back(std::move(h));
}

void TopPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff171a20));
    g.setColour(juce::Colour(0xff2a2f38));
    g.drawRect(getLocalBounds());
}

void TopPanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    constexpr int kBtnH = 24, kGap = 4;

    auto group = [&](juce::Label& title, int w) {
        auto r = area.removeFromLeft(w);
        auto t = r.removeFromTop(16);
        title.setBounds(t);
        area.removeFromLeft(10);
        return r;
    };

    auto fileR = group(fileTitle, 140);
    int fy = fileR.getY();
    for (auto* b : { &restoreBtn, &loadBtn, &saveBtn, &saveAsBtn }) { b->setBounds(fileR.getX(), fy, fileR.getWidth(), kBtnH); fy += kBtnH + kGap; }

    auto defR = group(defaultsTitle, 340);
    const int half = 168;
    int dyL = defR.getY(), dyR = defR.getY();
    for (size_t i = 0; i < defaultRows.size(); ++i) {
        auto& row = defaultRows[i];
        const bool leftCol = i % 2 == 0;
        const int x = defR.getX() + (leftCol ? 0 : half);
        int& y = leftCol ? dyL : dyR;
        row.labelComp->setBounds(x, y, 72, kBtnH);
        if (row.text) row.text->setBounds(x + 72, y, 92, kBtnH);
        if (row.combo) row.combo->setBounds(x + 72, y, 92, kBtnH);
        y += kBtnH + kGap;
    }

    auto editR = group(editTitle, 208);
    int ey = editR.getY();
    addLabel_.setBounds(editR.getX(), ey, 30, kBtnH);
    addName.setBounds(editR.getX() + 34, ey, 34, kBtnH);
    addType.setBounds(editR.getX() + 72, ey, 92, kBtnH);
    addBtn.setBounds(editR.getX() + 168, ey, 26, kBtnH);
    ey += kBtnH + kGap;
    deleteBtn.setBounds(editR.getX(), ey, 84, kBtnH);
    mergeBtn.setBounds(editR.getX() + 92, ey, 84, kBtnH);
    ey += kBtnH + kGap;
    dtLabel_.setBounds(editR.getX(), ey, 72, kBtnH);
    datatypeCombo.setBounds(editR.getX() + 72, ey, 122, kBtnH);
}
