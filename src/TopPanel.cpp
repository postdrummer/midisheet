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
    undoBtn.setButtonText("Undo");
    redoBtn.setButtonText("Redo");
    undoBtn.onClick = [this] { grid.undo(); };
    redoBtn.onClick = [this] { grid.redo(); };
    restoreBtn.setButtonText("Restore Defaults");
    loadBtn.setButtonText("Load");
    saveBtn.setButtonText("Save");
    saveAsBtn.setButtonText("Save As...");
    restoreBtn.onClick = [this] {
        auto* w = new juce::AlertWindow("Restore Defaults",
                                        "Would you like to restore defaults?",
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
    saveBtn.onClick = [this] { save(); };
    saveAsBtn.onClick = [this] { pickFile(true); };
    for (auto* b : { &undoBtn, &redoBtn, &restoreBtn, &loadBtn, &saveBtn, &saveAsBtn }) { b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2f38)); b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe6e9ee)); style(*b); addAndMakeVisible(b); }
    watchHover(undoBtn, "Undo the last edit.");
    watchHover(redoBtn, "Redo the last undone edit.");
    watchHover(restoreBtn, "Reset the sheet to the built-in default columns and content.");
    watchHover(loadBtn, "Load a sheet from a .json/.midisheet file.");
    watchHover(saveBtn, "Save the current sheet (Cmd/Ctrl+S).");
    watchHover(saveAsBtn, "Save the current sheet to a new file.");

    // ---- Defaults ----
    // (label, type, usesCombo). Numeric types get a text box; a few use
    // dropdowns with relevant choices, as requested.
    struct RowSpec { const char* label; arp::ColumnType type; bool combo; };
    const RowSpec specs[] = {
        { "Note",     arp::ColumnType::Note,     true  }, // boolean On/Off with empty=on
        { "Pitch",    arp::ColumnType::Pitch,    true  }, // combo: Midi In + note names
        { "Velocity", arp::ColumnType::Velocity, true  }, // combo: Midi In + 0-128
        { "Gate",     arp::ColumnType::Gate,     false }, // text box + %
        { "Chance",   arp::ColumnType::Chance,   false }, // text box + %
        { "Shift",    arp::ColumnType::Shift,    false },
        { "Octave",   arp::ColumnType::Octave,   false },
        { "Time",     arp::ColumnType::Time,     true  }, // two combos: style + division
        { "Repeat",   arp::ColumnType::Repeat,   true  }, // combo: 1-16
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
                    applyDefault(t, idx == 0 ? 1.0 : 0.0);
                };
            } else if (s.type == arp::ColumnType::Pitch) {
                row.combo->addItem("Midi In", 1);
                for (int n = 0; n < 128; ++n)
                    row.combo->addItem(juce::MidiMessage::getMidiNoteName(n, true, true, 4), n + 2);
                row.combo->onChange = [this, t = s.type, cb = row.combo.get()] {
                    const int idx = cb->getSelectedItemIndex();
                    applyDefault(t, idx <= 0 ? emptyComboValue(t) : (double)(idx - 1));
                };
            } else if (s.type == arp::ColumnType::Velocity) {
                row.combo->addItem("Midi In", 1);
                for (int i = 0; i <= 128; ++i)
                    row.combo->addItem(juce::String(i), i + 2);
                row.combo->onChange = [this, t = s.type, cb = row.combo.get()] {
                    const int idx = cb->getSelectedItemIndex();
                    applyDefault(t, idx <= 0 ? emptyComboValue(t) : (double)(idx - 1));
                };
            } else if (s.type == arp::ColumnType::Time) {
                // Two independent combos: style (Equal/Dotted/Triplet) + division (1/1..1/28)
                row.combo->addItem("Equal", 1);
                row.combo->addItem("Dotted", 2);
                row.combo->addItem("Triplet", 3);
                row.combo->onChange = [this, t = s.type, cb = row.combo.get(), row2 = &row] {
                    const int divIdx = row2->combo2 ? row2->combo2->getSelectedItemIndex() : 4;
                    applyDefault(t, divIdx);
                };
                row.combo2 = std::make_unique<DragCombo>();
                const char* divs[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/28" };
                for (int i = 0; i < 8; ++i) row.combo2->addItem(divs[i], i + 1);
                row.combo2->onChange = [this, t = s.type, cb = row.combo2.get()] {
                    const int idx = cb->getSelectedItemIndex();
                    applyDefault(t, idx);
                };
                addAndMakeVisible(*row.combo2);
            } else if (s.type == arp::ColumnType::Repeat) {
                for (int i = 1; i <= 16; ++i)
                    row.combo->addItem(juce::String(i), i);
                row.combo->onChange = [this, t = s.type, cb = row.combo.get()] {
                    const int idx = cb->getSelectedItemIndex();
                    if (idx >= 0) applyDefault(t, (double)(idx + 1));
                };
            }
            addAndMakeVisible(*row.combo);
        } else {
            row.text = std::make_unique<juce::TextEditor>();
            row.text->setFont(monoFont());
            row.text->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e2127));
            row.text->setColour(juce::TextEditor::textColourId, juce::Colour(0xffe6e9ee));
            const bool isPercent = (s.type == arp::ColumnType::Gate || s.type == arp::ColumnType::Chance);
            row.text->onReturnKey = [this, t = s.type, te = row.text.get(), isPercent] {
                auto text = te->getText().trim();
                if (isPercent) text = text.replace("%", "").trim();
                if (text.isNotEmpty()) applyDefault(t, text.getDoubleValue());
            };
            addAndMakeVisible(*row.text);
        }
        defaultRows.push_back(std::move(row));
    }
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
        grid.deleteSelection();
        refreshFromSheet();
    };
    mergeBtn.setButtonText("Merge");
    mergeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2f38));
    mergeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe6e9ee));
    mergeBtn.onClick = [this] {
        const int dc = grid.getCol() - 1;
        if (dc < 0) return;
        const auto sel = grid.selRows();
        const int r0 = sel.first;
        const int r1 = sel.second;
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

void TopPanel::save()
{
    if (currentFile != juce::File{}) {
        currentFile.replaceWithText(juce::JSON::toString(proc.getSheet().toVar(), true));
    } else {
        pickFile(true);
    }
}

void TopPanel::pickFile(bool save)
{
    fileChooser_ = std::make_unique<juce::FileChooser>(save ? "Save sheet" : "Load sheet", juce::File{}, "*.json *.midisheet");
    const auto mode = save ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode;
    fileChooser_->launchAsync(mode, [this, save](const juce::FileChooser& chooser) {
        const auto f = chooser.getResult();
        if (f == juce::File{}) return;
        if (save) {
            f.replaceWithText(juce::JSON::toString(proc.getSheet().toVar(), true));
            currentFile = f;
            return;
        }
        if (juce::var v; juce::JSON::parse(f.loadFileAsString(), v).wasOk()) {
            proc.getSheet().fromVar(v);
            currentFile = f;
            proc.sheetChanged();
            refreshFromSheet();
        }
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

void TopPanel::refreshDefaultsRow(TypeRow& row) const
{
    double v = 0.0; bool found = false;
    for (int c = 0; c < proc.getSheet().getNumColumns(); ++c)
        if (proc.getSheet().getColumn(c).type == row.type) { v = proc.getSheet().getColumn(c).defaultValue; found = true; break; }
    if (!found) return;
    if (row.combo) {
        int idx = 0;
        if (row.type == arp::ColumnType::Note)
            idx = (v >= 0.5) ? 0 : 1; // 1=On, 0=Off
        else if (row.type == arp::ColumnType::Pitch)
            idx = (v < 0) ? 0 : (int) v + 1;
        else if (row.type == arp::ColumnType::Velocity)
            idx = (v < 0) ? 0 : (int) v + 1;
        else if (row.type == arp::ColumnType::Time)
            idx = 0; // Default to "Equal" for style
        else if (row.type == arp::ColumnType::Repeat)
            idx = (int) v - 1;
        idx = juce::jlimit(0, row.combo->getNumItems() - 1, idx);
        row.combo->setSelectedItemIndex(idx, juce::dontSendNotification);
        row.combo->resized();
        // Sync the second Time combo (division) — independent of style
        if (row.combo2) {
            int divIdx = 4; // Default to 1/16 (index 4)
            if (row.type == arp::ColumnType::Time && v > 0)
                divIdx = (int) v;
            divIdx = juce::jlimit(0, row.combo2->getNumItems() - 1, divIdx);
            row.combo2->setSelectedItemIndex(divIdx, juce::dontSendNotification);
            row.combo2->resized();
        }
    } else {
        const bool isPercent = (row.type == arp::ColumnType::Gate || row.type == arp::ColumnType::Chance);
        const juce::String text = isPercent
            ? juce::String(juce::roundToInt(v)) + "%"
            : juce::String(v, v == std::floor(v) ? 0 : 2);
        row.text->setText(text, juce::dontSendNotification);
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
        void mouseExit(const juce::MouseEvent&) override { bar.setText("Hover for info.", juce::dontSendNotification); }
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
    // Undo/Redo side by side
    const int halfBtn = (fileR.getWidth() - kGap) / 2;
    undoBtn.setBounds(fileR.getX(), fy, halfBtn, kBtnH);
    redoBtn.setBounds(fileR.getX() + halfBtn + kGap, fy, halfBtn, kBtnH);
    fy += kBtnH + kGap;
    restoreBtn.setBounds(fileR.getX(), fy, fileR.getWidth(), kBtnH);
    fy += kBtnH + kGap;
    loadBtn.setBounds(fileR.getX(), fy, fileR.getWidth(), kBtnH);
    fy += kBtnH + kGap;
    // Save/Save As side by side
    saveBtn.setBounds(fileR.getX(), fy, halfBtn, kBtnH);
    saveAsBtn.setBounds(fileR.getX() + halfBtn + kGap, fy, halfBtn, kBtnH);

    auto defR = group(defaultsTitle, 340);
    const int half = 168;
    int dyL = defR.getY(), dyR = defR.getY();
    for (size_t i = 0; i < defaultRows.size(); ++i) {
        auto& row = defaultRows[i];
        const bool leftCol = i % 2 == 0;
        const int x = defR.getX() + (leftCol ? 0 : half);
        int& y = leftCol ? dyL : dyR;
        row.labelComp->setBounds(x, y, 72, kBtnH);
        if (row.text) {
            row.text->setBounds(x + 72, y, 92, kBtnH);
        }
        if (row.combo) {
            if (row.combo2) {
                // Time row: two combos side by side
                row.combo->setBounds(x + 72, y, 70, kBtnH);
                row.combo2->setBounds(x + 142, y, 70, kBtnH);
            } else {
                row.combo->setBounds(x + 72, y, 92, kBtnH);
            }
        }
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
