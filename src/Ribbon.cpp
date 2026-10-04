#include "Ribbon.h"

#include <juce_core/juce_core.h>

#include <regex>

namespace {

std::unique_ptr<juce::TextButton> mkBtn(const juce::String& s, std::function<void()> fn)
{
    auto b = std::make_unique<juce::TextButton>(s);
    b->onClick = std::move(fn);
    return b;
}

std::unique_ptr<juce::ToggleButton> mkToggle(const juce::String& s, bool on, std::function<void(bool)> fn)
{
    auto t = std::make_unique<juce::ToggleButton>(s);
    t->setToggleState(on, juce::dontSendNotification);
    t->onClick = [t_ = t.get(), f = std::move(fn)] { f(t_->getToggleState()); };
    return t;
}

std::unique_ptr<juce::ComboBox> mkCombo(const juce::StringArray& items, int sel, std::function<void(int)> fn)
{
    auto c = std::make_unique<juce::ComboBox>();
    for (int i = 0; i < items.size(); ++i)
        c->addItem(items[i], i + 1);
    c->setSelectedItemIndex(sel, juce::dontSendNotification);
    c->onChange = [c_ = c.get(), f = std::move(fn)] { f(c_->getSelectedItemIndex()); };
    return c;
}

} // namespace

Ribbon::Ribbon(MidisheetAudioProcessor& p, TrackerGrid& grid, juce::TextEditor& formula)
    : proc(p), grid_(grid), formula_(formula)
{
    auto& t = tabs;
    t.setOutline(0);
    t.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff2b2b2b));
    t.getTabbedButtonBar().setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colour(0xffd6d6d6));
    t.getTabbedButtonBar().setColour(juce::TabbedButtonBar::frontTextColourId, juce::Colours::white);
    t.getTabbedButtonBar().setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colour(0xff217346));
    t.getTabbedButtonBar().setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(t);

    auto mkTab = [&](const char* name, std::function<void(RibbonTab&)> build) -> RibbonTab* {
        auto* page = new RibbonTab();
        t.addTab(name, juce::Colour(0xff2b2b2b), page, true);
        build(*page);
        return page;
    };
    auto& home = *mkTab("Home", [this](RibbonTab& tab) { buildHome(tab); });
    auto& columns = *mkTab("Columns", [this](RibbonTab& tab) { buildColumns(tab); });
    auto& arp = *mkTab("Arpeggiator", [this](RibbonTab& tab) { buildArp(tab); });
    auto& formulas = *mkTab("Formulas", [this](RibbonTab& tab) { buildFormulas(tab); });
    auto& view = *mkTab("View", [this](RibbonTab& tab) { buildView(tab); });
    auto& midi = *mkTab("MIDI", [this](RibbonTab& tab) { buildMidi(tab); });
    auto& file = *mkTab("File", [this](RibbonTab& tab) { buildFile(tab); });

    (void)home; (void)columns; (void)arp; (void)formulas; (void)view; (void)midi; (void)file;
}

void Ribbon::resized()
{
    tabs.setBounds(getLocalBounds());
}

// ---------------------------------------------------------------- Home

void Ribbon::buildHome(RibbonTab& tab)
{
    tab.addItem(mkBtn("Cut", [this] { grid_.copySelection(); grid_.clearSelection(); }), 64);
    tab.addItem(mkBtn("Copy", [this] { grid_.copySelection(); }), 64);
    tab.addItem(mkBtn("Paste", [this] { grid_.setPasteMode(0); grid_.pasteIntoSelection(); }), 64);

    auto pasteSpecial = std::make_unique<juce::TextButton>("Paste Special");
    pasteSpecial->onClick = [this, ps = pasteSpecial.get()] {
        auto m = std::make_unique<juce::PopupMenu>();
        m->addItem(1, "All");
        m->addItem(2, "Formulas only");
        m->addItem(3, "Values only");
        auto options = juce::PopupMenu::Options().withTargetComponent(ps);
        m->showMenuAsync(options, [this](int result) {
            if (result == 1) grid_.setPasteMode(0);
            else if (result == 2) grid_.setPasteMode(1);
            else if (result == 3) grid_.setPasteMode(2);
            else return;
            grid_.pasteIntoSelection();
        });
        m.release(); // leaked-by-callback: PopupMenu keeps itself while modal
    };
    tab.addItem(std::move(pasteSpecial), 96);

    tab.addItem(mkBtn("Ins row", [this] {
        const int r = grid_.getRow();
        grid_.performEdit([this, r] { proc.getSheet().insertRow(r); });
    }), 82);
    tab.addItem(mkBtn("Del row", [this] {
        const int r = grid_.getRow();
        grid_.performEdit([this, r] { proc.getSheet().deleteRow(r); });
    }), 82);

    tab.addItem(mkBtn("Ins col", [this] {
        const int dCol = grid_.getCol() - 1;
        if (dCol < 0) return;
        grid_.performEdit([this, dCol] {
            auto& s = proc.getSheet();
            const int added = s.addColumn(arp::ColumnType::Number, "Custom");
            s.moveColumn(added, dCol);
        });
    }), 80);
    tab.addItem(mkBtn("Del col", [this] {
        const int dCol = grid_.getCol() - 1;
        if (dCol < 0) return;
        grid_.performEdit([this, dCol] { proc.getSheet().removeColumn(dCol); });
    }), 80);

    tab.addItem(mkBtn("Clear", [this] { grid_.clearSelection(); }), 64);

    juce::StringArray types;
    for (int i = 0; i < 12; ++i)
        types.add(arp::columnTypeName(static_cast<arp::ColumnType>(i)));
    tab.addItem(mkCombo(types, 0, [this](int idx) {
        const int dCol = grid_.getCol() - 1;
        if (dCol < 0) return;
        grid_.performEdit([this, dCol, idx] {
            proc.getSheet().setColumnType(dCol, static_cast<arp::ColumnType>(idx));
        });
    }), 100);
}

// ---------------------------------------------------------------- Columns

void Ribbon::buildColumns(RibbonTab& tab)
{
    juce::StringArray types;
    for (int i = 0; i < 12; ++i)
        types.add(arp::columnTypeName(static_cast<arp::ColumnType>(i)));

    tab.addItem(mkCombo(types, 0, [this](int idx) {
        grid_.performEdit([this, idx] {
            proc.getSheet().addColumn(static_cast<arp::ColumnType>(idx));
        });
    }), 110);

    tab.addItem(mkBtn("Show all", [this] {
        grid_.performEdit([this] {
            auto& s = proc.getSheet();
            for (int c = 0; c < s.getNumColumns(); ++c) s.setColumnVisible(c, true);
        });
    }), 72);
    tab.addItem(mkBtn("Hide all", [this] {
        grid_.performEdit([this] {
            auto& s = proc.getSheet();
            for (int c = 0; c < s.getNumColumns(); ++c) s.setColumnVisible(c, false);
        });
    }), 72);
    tab.addItem(mkBtn("Invert", [this] {
        grid_.performEdit([this] {
            auto& s = proc.getSheet();
            for (int c = 0; c < s.getNumColumns(); ++c)
                s.setColumnVisible(c, !s.getColumn(c).visible);
        });
    }), 72);

    colTypeCombo_ = std::make_unique<juce::ComboBox>();
    for (int i = 0; i < types.size(); ++i)
        colTypeCombo_->addItem(types[i], i + 1);
    colTypeCombo_->setSelectedItemIndex(0, juce::dontSendNotification);
    colTypeCombo_->onChange = [this] {
        const int dCol = grid_.getCol() - 1;
        if (dCol < 0) return;
        grid_.performEdit([this, dCol] {
            proc.getSheet().setColumnType(dCol, static_cast<arp::ColumnType>(colTypeCombo_->getSelectedItemIndex()));
        });
    };
    tab.addExisting(*colTypeCombo_, 100);

    colNameEditor_ = std::make_unique<juce::TextEditor>();
    colNameEditor_->setJustification(juce::Justification::centredLeft);
    colNameEditor_->onReturnKey = [this] {
        const int dCol = grid_.getCol() - 1;
        if (dCol < 0) return;
        const auto name = colNameEditor_->getText().trim().toStdString();
        grid_.performEdit([this, dCol, name] {
            if (!name.empty())
                proc.getSheet().setColumnName(dCol, name);
        });
    };
    tab.addExisting(*colNameEditor_, 110);

    colDefaultEditor_ = std::make_unique<juce::TextEditor>();
    colDefaultEditor_->setJustification(juce::Justification::centredLeft);
    colDefaultEditor_->onReturnKey = [this] {
        const int dCol = grid_.getCol() - 1;
        if (dCol < 0) return;
        const double v = colDefaultEditor_->getText().getDoubleValue();
        grid_.performEdit([this, dCol, v] { proc.getSheet().setColumnDefault(dCol, v); });
    };
    tab.addExisting(*colDefaultEditor_, 90);

    colVisibleToggle_ = std::make_unique<juce::ToggleButton>("Visible");
    colVisibleToggle_->onClick = [this] {
        const int dCol = grid_.getCol() - 1;
        if (dCol < 0) return;
        const bool on = colVisibleToggle_->getToggleState();
        grid_.performEdit([this, dCol, on] { proc.getSheet().setColumnVisible(dCol, on); });
    };
    tab.addExisting(*colVisibleToggle_, 80);

    tab.addItem(mkBtn("Move <", [this] {
        const int dc = grid_.getCol() - 1;
        if (dc <= 0) return;
        grid_.performEdit([this, dc] { proc.getSheet().moveColumn(dc, dc - 1); });
        grid_.selectCell(grid_.getRow(), dc);
    }), 74);
    tab.addItem(mkBtn("Move >", [this] {
        const int dc = grid_.getCol() - 1;
        if (dc < 0 || dc >= proc.getSheet().getNumColumns() - 1) return;
        grid_.performEdit([this, dc] { proc.getSheet().moveColumn(dc, dc + 1); });
        grid_.selectCell(grid_.getRow(), dc + 2);
    }), 74);
    tab.addItem(mkBtn("Rename...", [this] {
        const int dc = grid_.getCol() - 1;
        if (dc < 0) return;
        auto* w = new juce::AlertWindow("Rename column", {}, juce::AlertWindow::NoIcon);
        w->addTextEditor("name", proc.getSheet().getColumn(dc).name, "Name");
        w->addButton("OK", 1);
        w->addButton("Cancel", 0);
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w, dc](int b) {
            if (b == 1) {
                const auto name = w->getTextEditorContents("name").trim().toStdString();
                if (!name.empty())
                    grid_.performEdit([this, dc, name] { proc.getSheet().setColumnName(dc, name); });
            }
            delete w;
        }), false);
    }), 92);
    tab.addItem(mkBtn("Remove", [this] {
        const int dc = grid_.getCol() - 1;
        if (dc < 0) return;
        grid_.performEdit([this, dc] { proc.getSheet().removeColumn(dc); });
        grid_.selectCell(grid_.getRow(), juce::jmin(dc + 1, proc.getSheet().getNumColumns()));
    }), 84);
}

// ---------------------------------------------------------------- Arpeggiator

void Ribbon::buildArp(RibbonTab& tab)
{
    juce::StringArray modes{"Up", "Down", "Up-Down", "Down-Up", "Random", "Order", "Chord"};
    tab.addItem(mkCombo(modes, 0, [this](int idx) {
        proc.getAPVTS().getParameter("mode")->setValueNotifyingHost(idx / 6.0f);
    }), 96);

    juce::StringArray rates{"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                            "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T", "1/64T"};
    tab.addItem(mkCombo(rates, 4, [this](int idx) {
        proc.getAPVTS().getParameter("rate")->setValueNotifyingHost(idx / 13.0f);
    }), 80);

    tab.addItem(std::make_unique<DragField>(0.0f, 0.0f, 1.0f, 2, [this](float v) {
        proc.getAPVTS().getParameter("swing")->setValueNotifyingHost(v);
    }, "Swing"), 64);

    tab.addItem(std::make_unique<DragField>(50.0f, 0.0f, 100.0f, 0, [this](float v) {
        proc.getAPVTS().getParameter("gate")->setValueNotifyingHost(v / 100.0f);
    }, "Gate"), 64);

    tab.addItem(std::make_unique<DragField>(1.0f, 1.0f, 8.0f, 0, [this](float v) {
        auto p = proc.getAPVTS().getParameter("octaveRange");
        p->setValueNotifyingHost((v - 1.0f) / 7.0f);
    }, "Octave range"), 48);

    tab.addItem(std::make_unique<DragField>(1.0f, 0.1f, 16.0f, 1, [this](float v) {
        auto* p = proc.getAPVTS().getParameter("length");
        const juce::NormalisableRange<float> range(0.1f, 16.0f);
        p->setValueNotifyingHost(range.convertTo0to1(v));
    }, "Length"), 72);
}

// ---------------------------------------------------------------- Formulas

void Ribbon::buildFormulas(RibbonTab& tab)
{
    juce::StringArray fns{"Pick...", "IF(", "MOD(", "SUM(", "AVG(", "AVERAGE(", "MIN(", "MAX(",
                           "ABS(", "ROUND(", "FLOOR(", "CEIL(", "RANDOM(", "NOTE(", "PREV("};
    tab.addItem(mkCombo(fns, 0, [this, fns](int idx) {
        if (idx <= 0) return;
        formula_.insertTextAtCaret(fns[idx]);
        formula_.grabKeyboardFocus();
    }), 96);

    tab.addItem(mkBtn("Toggle $A$1", [this] {
        auto text = formula_.getText().toStdString();
        if (text.find('$') != std::string::npos) {
            std::string out;
            for (char c : text)
                if (c != '$') out += c;
            formula_.setText(out, true);
            return;
        }
        // Otherwise add $ around the column letters and row digits of any
        // ref-like token "A1"/"AB23" (foo("...") functions are not touched
        // because they are followed by '(').
        std::string out;
        size_t i = 0;
        while (i < text.size()) {
            if (std::isalpha(static_cast<unsigned char>(text[i])) &&
                (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i - 1])))) {
                size_t j = i;
                while (j < text.size() && std::isalpha(static_cast<unsigned char>(text[j]))) ++j;
                size_t k = j;
                while (k < text.size() && std::isdigit(static_cast<unsigned char>(text[k]))) ++k;
                const bool looksLikeRef = k > j && (k >= text.size() || text[k] != '(');
                if (looksLikeRef) {
                    out += '$';
                    out.append(text, i, j - i);
                    out += '$';
                    out.append(text, j, k - j);
                } else {
                    out.append(text, i, k - i);
                }
                i = k;
            } else {
                out += text[i++];
            }
        }
        formula_.setText(out, true);
    }), 96);

    tab.addItem(mkBtn("Error check", [this] {
        const auto err = grid_.cellError();
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "Error Check", err.isEmpty() ? "No error." : err);
    }), 88);

    tab.addItem(mkBtn("Trace: prec.", [this] { traceRefs(true); }), 96);
    tab.addItem(mkBtn("Trace: dep.", [this] { traceRefs(false); }), 96);
}

void Ribbon::traceRefs(bool precedents)
{
    auto& sheet = proc.getSheet();
    const int dCol = grid_.getCol() - 1;
    if (dCol < 0 || dCol >= sheet.getNumColumns())
        return;
    const int row = grid_.getRow();

    juce::StringArray hits;
    if (precedents) {
        const std::regex re("(\\$?[A-Za-z]+\\$?[1-9][0-9]*)");
        const auto text = sheet.getCellFormula(dCol, row);
        for (std::sregex_iterator it(text.begin(), text.end(), re); it != std::sregex_iterator{}; ++it)
            hits.add(juce::String((*it)[1].str()));
    } else {
        const juce::String ref = juce::String(arp::columnLetters(dCol)) + juce::String(row + 1);
        for (int c = 0; c < sheet.getNumColumns(); ++c)
            for (int r = 0; r < sheet.getNumRows(); ++r) {
                const auto f = sheet.getCellFormula(c, r);
                if (!f.empty() && juce::String(f).contains(ref))
                    hits.add(juce::String(arp::columnLetters(c)) + juce::String(r + 1) + ": " + juce::String(f));
            }
    }
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                           precedents ? "Precedents" : "Dependents (this ref)",
                                           hits.joinIntoString("\n"));
}

// ---------------------------------------------------------------- View

void Ribbon::buildView(RibbonTab& tab)
{
    tab.addItem(std::make_unique<juce::ToggleButton>("Gridlines"), 90);
    tab.addItem(std::make_unique<juce::ToggleButton>("Row numbers"), 100);
    tab.addItem(std::make_unique<juce::ToggleButton>("Headers"), 84);
    tab.addItem(std::make_unique<juce::ToggleButton>("Freeze top row"), 110);

    juce::StringArray colours{"Amber", "Blue", "Red", "Green"};
    tab.addItem(mkCombo(colours, 0, [this](int idx) {
        switch (idx) {
            case 0: grid_.setPlayheadColour(juce::Colour(0xffe0b050)); break;
            case 1: grid_.setPlayheadColour(juce::Colour(0xff6ea8fe)); break;
            case 2: grid_.setPlayheadColour(juce::Colour(0xffff6f5e)); break;
            default: grid_.setPlayheadColour(juce::Colour(0xff7dde7d)); break;
        }
    }), 84);

    tab.addItem(std::make_unique<DragField>(1.0f, 0.5f, 3.0f, 2, [this](float v) {
        grid_.setZoom(v);
    }, "Zoom"), 64);

    tab.addItem(mkBtn("Auto-fit", [this] { grid_.autoFitColumnWidths(); }), 78);
}

// ---------------------------------------------------------------- MIDI

void Ribbon::buildMidi(RibbonTab& tab)
{
    juce::StringArray chans{"All"};
    for (int c = 1; c <= 16; ++c) chans.add(juce::String(c));
    tab.addItem(mkCombo(chans, 0, [this](int idx) { proc.midiInputChannel.store(idx); }), 72);

    tab.addItem(std::make_unique<DragField>(0.0f, 0.0f, 127.0f, 0,
                                            [this](float v) { proc.midiMinNote.store(static_cast<int>(v)); }, "Min note"), 52);
    tab.addItem(std::make_unique<DragField>(127.0f, 0.0f, 127.0f, 0,
                                            [this](float v) { proc.midiMaxNote.store(static_cast<int>(v)); }, "Max note"), 52);

    juce::StringArray curves{"Linear", "Soft", "Hard", "Full"};
    tab.addItem(mkCombo(curves, 0, [this](int idx) { proc.midiCurve.store(idx); }), 84);

    tab.addItem(mkCombo(juce::StringArray({"1", "2", "3", "4", "5", "6", "7", "8",
                                          "9", "10", "11", "12", "13", "14", "15", "16"}),
                        0, [this](int idx) { proc.midiOutputChannel.store(idx + 1); }), 76);

    juce::StringArray routes{"Replace", "Augment"};
    tab.addItem(mkCombo(routes, 0, [this](int idx) { proc.midiRouting.store(idx); }), 90);

    tab.addItem(std::make_unique<juce::ToggleButton>("Thru"), 70);
}

// ---------------------------------------------------------------- File

void Ribbon::buildFile(RibbonTab& tab)
{
    tab.addItem(mkBtn("Save", [this] { saveLoadFile(/*save*/ true); }), 64);
    tab.addItem(mkBtn("Load", [this] { saveLoadFile(false); }), 64);
    tab.addItem(mkBtn("Import", [this] { saveLoadFile(false); }), 64);
    tab.addItem(mkBtn("Export", [this] { saveLoadFile(true); }), 66);
    tab.addItem(mkBtn("Export CSV", [this] { exportCsv(); }), 84);
    tab.addItem(mkBtn("Undo", [this] { grid_.undo(); }), 64);
    tab.addItem(mkBtn("Redo", [this] { grid_.redo(); }), 64);
    tab.addItem(mkBtn("Reset", [this] {
        grid_.performEdit([this] { proc.getSheet().clear(); });
        grid_.repaint();
    }), 64);
}

void Ribbon::saveLoadFile(bool save)
{
    fileChooser_ = std::make_unique<juce::FileChooser>(save ? "Save sheet" : "Load sheet",
                                                       juce::File{}, "*.json *.midisheet");
    const auto mode = save ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode;
    fileChooser_->launchAsync(mode, [this, save](const juce::FileChooser& chooser) {
        const auto f = chooser.getResult();
        if (f == juce::File{})
            return;
        handleSheetFile(f, save);
    });
}

void Ribbon::handleSheetFile(const juce::File& file, bool save)
{
    if (save) {
        file.replaceWithText(juce::JSON::toString(proc.getSheet().toVar(), true));
        return;
    }
    const juce::String text = file.loadFileAsString();
    juce::var v;
    auto res = juce::JSON::parse(text, v);
    if (res.failed())
        return;
    proc.getSheet().fromVar(v);
    proc.sheetChanged();
}

void Ribbon::exportCsv()
{
    auto& sheet = proc.getSheet();
    juce::String out;
    for (int r = 0; r < sheet.getNumRows(); ++r) {
        for (int c = 0; c < sheet.getNumColumns(); ++c) {
            if (c > 0) out << ',';
            const auto f = sheet.getCellFormula(c, r);
            if (!f.empty())
                out << juce::String(f);
            else if (sheet.hasCellValue(c, r))
                out << juce::String(sheet.getCellValue(c, r));
        }
        out << '\n';
    }
    fileChooser_ = std::make_unique<juce::FileChooser>("Export CSV", juce::File{}, "*.csv");
    const auto t = out;
    fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [this, t](const juce::FileChooser& chooser) {
                        const auto file = chooser.getResult();
                        if (file != juce::File{})
                            file.replaceWithText(t);
                    });
}

void Ribbon::activeColumnChanged()
{
    const int numCols = proc.getSheet().getNumColumns();
    const int dc = grid_.getCol() - 1;
    const bool active = dc >= 0 && dc < numCols;
    colTypeCombo_->setEnabled(active);
    colNameEditor_->setEnabled(active);
    colDefaultEditor_->setEnabled(active);
    colVisibleToggle_->setEnabled(active);
    if (!active) {
        colNameEditor_->setText({}, false);
        colDefaultEditor_->setText({}, false);
        return;
    }
    const auto& col = proc.getSheet().getColumn(dc);
    colNameEditor_->setText(juce::String(col.name), false);
    colTypeCombo_->setSelectedItemIndex(static_cast<int>(col.type), juce::dontSendNotification);
    const double d = col.defaultValue;
    colDefaultEditor_->setText(juce::String(d, d == std::floor(d) ? 0 : 2), false);
    colVisibleToggle_->setToggleState(col.visible, juce::dontSendNotification);
}
