#include "PluginEditor.h"

namespace {

} // namespace

MidisheetAudioProcessorEditor::MidisheetAudioProcessorEditor(MidisheetAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), grid(p), ribbon(p, grid, formulaBar)
{
    (void)p.getAPVTS(); // APVTS params remain available to the ribbon and for host automation.

    // Formula bar
    const auto mono = juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain);
    nameBox.setFont(juce::Font(mono));
    nameBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe0b050));
    nameBox.setJustification(juce::Justification::centredRight);
    nameBox.onReturnKey = [this] { grid.jumpTo(nameBox.getText()); grid.grabKeyboardFocus(); };
    nameBox.onEscapeKey = [this] { showSelectedCell(); grid.grabKeyboardFocus(); };
    nameBox.onFocusLost = [this] { showSelectedCell(); };
    formulaBar.setFont(juce::Font(mono));
    formulaBar.setTextToShowWhenEmpty("(empty: uses the default)", juce::Colour(0xff646c78));
    formulaBar.onReturnKey = [this] { commitFormula(); grid.grabKeyboardFocus(); };
    formulaBar.onEscapeKey = [this] { showSelectedCell(); grid.grabKeyboardFocus(); };
    formulaBar.onFocusLost = [this] { commitFormula(); hideSuggestions(); };
    formulaBar.onTextChange = [this] { updateSuggestions(); };
    formulaBar.acVisibleHook = [this] { return acBox.isVisible(); };
    formulaBar.acUp = [this] { navSuggestions(-1); };
    formulaBar.acDown = [this] { navSuggestions(1); };
    formulaBar.acAccept = [this] { acceptSuggestion(); };
    formulaBar.acDismiss = [this] { hideSuggestions(); };
    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(nameBox);
    addAndMakeVisible(formulaBar);
    addAndMakeVisible(statusLabel);
    addChildComponent(acBox);
    acBox.onPick = [this](juce::String item) { acceptSuggestion(item); };

    addAndMakeVisible(ribbon);

    grid.onSelectionChanged = [this] { showSelectedCell(); };
    grid.onHoverStatus = [this](const juce::String&) { updateStatus(); };
    grid.onEditRequested = [this](const juce::String& initial) {
        formulaBar.setText(initial, false);
        formulaBar.grabKeyboardFocus();
        formulaBar.moveCaretToEnd();
    };
    addAndMakeVisible(grid);

    setResizable(true, true);
    setResizeLimits(560, 420, 1400, 1600);
    setSize(720, 680);
    showSelectedCell();
    fitHeightToRows();
    startTimerHz(10);
}

void MidisheetAudioProcessorEditor::visibilityChanged()
{
    // Focus can only be taken once the window is on screen.
    if (isShowing() && !formulaBar.hasKeyboardFocus(true))
        grid.grabKeyboardFocus();
}

void MidisheetAudioProcessorEditor::parentHierarchyChanged()
{
    visibilityChanged();
}

MidisheetAudioProcessorEditor::~MidisheetAudioProcessorEditor() {}

void MidisheetAudioProcessorEditor::showSelectedCell()
{
    if (!nameBox.hasKeyboardFocus(true))
        nameBox.setText(grid.cellName(), false);
    if (!formulaBar.hasKeyboardFocus(true))
        formulaBar.setText(grid.cellFormula(), false);

    updateStatus();
    ribbon.activeColumnChanged();
}

void MidisheetAudioProcessorEditor::updateStatus()
{
    const auto err = grid.cellError();
    if (!err.isEmpty()) {
        statusLabel.setText("Error: " + err, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff7a59));
        return;
    }

    const auto hover = grid.hoverStatusText();
    if (ribbonHover.isNotEmpty()) {
        statusLabel.setText(ribbonHover, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff646c78));
        return;
    }
    if (hover.isNotEmpty()) {
        statusLabel.setText(hover, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff646c78));
        return;
    }

    const int dCol = grid.getCol() - 1;
    auto& sheet = processorRef.getSheet();
    if (dCol >= 0 && dCol < sheet.getNumColumns()) {
        const auto& col = sheet.getColumn(dCol);
        const auto formula = grid.cellFormula();
        const auto value = grid.activeCellValueText();
        const bool hasContent = (formula.trim().isNotEmpty() && formula != "on" && formula != "off") ||
                                sheet.hasCellValue(dCol, grid.getRow());
        if (hasContent) {
            juce::String s = grid.cellName() + "  ·  " + juce::String(col.name) + "  ·  " +
                             arp::columnTypeName(col.type);
            if (formula.trim().isNotEmpty() && formula != "on" && formula != "off")
                s += "  ·  =" + formula;
            if (value.isNotEmpty())
                s += "  ·  " + value;
            statusLabel.setText(s, juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff646c78));
            return;
        }
    }

    statusLabel.setText({}, juce::dontSendNotification);
}

void MidisheetAudioProcessorEditor::commitFormula()
{
    grid.setCellFormula(formulaBar.getText());
    showSelectedCell();
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Formula autocomplete

void MidisheetAudioProcessorEditor::updateSuggestions()
{
    if (!formulaBar.hasKeyboardFocus(true)) {
        hideSuggestions();
        return;
    }

    const int caret = formulaBar.getCaretPosition();
    const juce::String head = formulaBar.getText().substring(0, caret);
    int start = head.length();
    while (start > 0 &&
           (juce::CharacterFunctions::isLetterOrDigit(head[start - 1]) || head[start - 1] == '_' || head[start - 1] == '$'))
        --start;
    const juce::String token = head.substring(start);

    acBox.items.clear();
    if (token.isNotEmpty()) {
        for (const auto& cand : candidatesForCompletion())
            if (cand.startsWithIgnoreCase(token) && !cand.equalsIgnoreCase(token))
                acBox.items.add(cand);
    }

    if (acBox.items.isEmpty()) {
        hideSuggestions();
        return;
    }

    acBox.updateContent();
    acBox.selectRow(0);
    const int h = juce::jmin(acBox.items.size() * 18 + 2, 160);
    acBox.setBounds(formulaBar.getX(), formulaBar.getBottom(), juce::jmin(formulaBar.getWidth(), 360), h);
    acBox.setVisible(true);
}

void MidisheetAudioProcessorEditor::hideSuggestions()
{
    acBox.setVisible(false);
    acBox.items.clear();
}

void MidisheetAudioProcessorEditor::acceptSuggestion(juce::String item)
{
    if (item.isEmpty() && acBox.getSelectedRow() >= 0)
        item = acBox.items[acBox.getSelectedRow()];
    if (item.isEmpty()) {
        hideSuggestions();
        return;
    }

    const int caret = formulaBar.getCaretPosition();
    const juce::String text = formulaBar.getText();
    const juce::String head = text.substring(0, caret);
    int start = head.length();
    while (start > 0 &&
           (juce::CharacterFunctions::isLetterOrDigit(head[start - 1]) || head[start - 1] == '_' || head[start - 1] == '$'))
        --start;
    const int replaceEnd = caret;
    formulaBar.setText(text.substring(0, start) + item + text.substring(replaceEnd), true);
    formulaBar.setCaretPosition(start + item.length());
    hideSuggestions();
    formulaBar.grabKeyboardFocus();
}

void MidisheetAudioProcessorEditor::navSuggestions(int dir)
{
    if (!acBox.isVisible() || acBox.items.isEmpty())
        return;
    int row = acBox.getSelectedRow() + dir;
    row = juce::jlimit(0, acBox.items.size() - 1, row);
    acBox.selectRow(row);
}

juce::StringArray MidisheetAudioProcessorEditor::candidatesForCompletion()
{
    static const char* const fns[] = {"IF",   "MOD", "SUM", "AVG", "AVERAGE", "MIN",
                                      "MAX",  "ABS", "ROUND", "FLOOR", "CEIL", "RANDOM", "RAND",
                                      "NOTE", "PREV"};
    juce::StringArray out;
    for (const char* f : fns)
        out.add(juce::String(f) + "(");

    for (const char* v : {"STEP", "ROW", "NOTE", "VELOCITY", "LENGTH", "CHANNEL", "PREV", "RANDOM", "RAND", "TRUE", "FALSE"})
        out.add(v);

    auto& sheet = processorRef.getSheet();
    for (int c = 0; c < sheet.getNumColumns(); ++c) {
        const auto& col = sheet.getColumn(c);
        out.add(juce::String(col.name) + "[");       // name-based ref lets you type the row expr
        out.add(juce::String(col.name));
        for (int r = 0; r < sheet.getNumRows(); ++r) // every visible cell ref in that column
            out.add(juce::String(arp::columnLetters(c)) + juce::String(r + 1));
    }
    out.removeDuplicates(true);
    return out;
}

void MidisheetAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff111317));
}

void MidisheetAudioProcessorEditor::timerCallback()
{
    const int v = processorRef.getSheetVersion();
    if (v != seenVersion || grid.getZoom() != seenZoom) {
        seenVersion = v;
        seenZoom = grid.getZoom();
        fitHeightToRows();
    }

    // Ribbon hover text: walk from the hovered component up through its
    // SettableTooltipClient ancestors and fall back to the control's label
    // (Button/ComboBox/ToggleButton/DragField value). Empty for components
    // that's aren't providing any kind of hint.
    juce::String hover;
    if (auto* c = juce::Desktop::getInstance().findComponentAt(juce::Desktop::getInstance().getMousePosition())) {
        for (auto* p = c; p != nullptr && p != &ribbon; p = p->getParentComponent()) {
            if (auto* stc = dynamic_cast<juce::SettableTooltipClient*>(p))
                if (stc->getTooltip().isNotEmpty()) { hover = stc->getTooltip(); break; }
            if (auto* b = dynamic_cast<juce::Button*>(p)) { hover = b->getButtonText(); break; }
            if (auto* cb = dynamic_cast<juce::ComboBox*>(p)) { hover = cb->getText(); break; }
        }
    }
    if (ribbonHover != hover) {
        ribbonHover = hover;
        updateStatus();
    }
}

void MidisheetAudioProcessorEditor::fitHeightToRows()
{
    constexpr int kChrome = 10 + 10 + (24 + 76 + 6) + (26 + 4) + (20 + 4); // margins + ribbon + bar + status
    const int rows = processorRef.getSheet().getNumRows();
    const int want = kChrome + grid.headerH() + (rows + 1) * grid.rowH(); // +1 for the "+ Add row" strip
    const int h = juce::jlimit(420, 1600, want);
    setSize(getWidth(), h);
}

void MidisheetAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(10);

    // Ribbon tabs + current tab's band.
    const int tabH = 24, bandH = 76;
    ribbon.setBounds(area.removeFromTop(tabH + bandH));
    area.removeFromTop(6);

    auto bar = area.removeFromTop(26);
    nameBox.setBounds(bar.removeFromLeft(70));
    formulaBar.setBounds(bar);
    area.removeFromTop(4);

    statusLabel.setBounds(area.removeFromBottom(20));
    area.removeFromBottom(4);
    grid.setBounds(area);
}
