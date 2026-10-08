#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

#include <array>
#include <functional>
#include <vector>

/**
 * Classic spreadsheet grid: one row per step, one column per sheet column.
 *
 * Layout (like Excel / LibreOffice Calc):
 *   - corner square (select all) over the row numbers
 *   - column headers: data type (top line), column name (bottom line);
 *     right-click for type/rename/hide/add/delete; click to select column
 *   - row numbers down the left; click to select the row
 *   - cells show the evaluated value for a preview chord (C E G, velocity 100,
 *     Up order); the formula behind the active cell is in the formula bar
 *   - the playing row is highlighted (tracker-style playhead)
 *
 * Keys (spreadsheet-style, all letter commands require a modifier):
 *   arrows / Tab / S-Tab  move / next / previous column
 *   PageUp / PageDown     page up / down
 *   Home / End            first / last step
 *   Enter / F2            edit formula
 *   Delete / Backspace    clear selection
 *   Space                 toggle step on/off
 *   = 0-9 - .             type a new formula (spreadsheet-style)
 *   Cmd/Ctrl+C / X / V    copy / cut / paste
 *   Cmd/Ctrl+Z            undo
 *   Cmd/Ctrl+Shift+Z      redo
 *   Cmd/Ctrl+Shift+= / -  add / delete rows or columns
 *   Escape                collapse selection to single cell
 */
class TrackerGrid : public juce::Component, private juce::Timer, public juce::SettableTooltipClient {
public:
    explicit TrackerGrid(MidisheetAudioProcessor&);

    int getRow() const { return row; }
    int getCol() const { return col; }
    juce::String cellName() const;                // e.g. "C3" (or "A1:C5" for a range)
    juce::String cellFormula() const;             // formula text of the active cell
    juce::String cellError() const;               // compile error of the active cell
    void setCellFormula(const juce::String& text); // commits (with undo)

    void jumpTo(const juce::String& ref);          // Name Box: "B4" -> select

    std::function<void()> onSelectionChanged;
    std::function<void(const juce::String& initialText)> onEditRequested;

    std::function<void(const juce::String&)> onHoverStatus;
    std::function<void()> onSaveRequested;
    juce::String hoverStatusText() const { return currentHoverStatus; }
    juce::String activeCellValueText() const;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // Public editing bailiwicks.
    void undo();
    void redo();
    void copySelection();
    void pasteIntoSelection();
    void clearSelection();
    void deleteSelection(); // context-sensitive: cells, rows, or columns
    void copyRowText(int r);
    void pasteRowText(int r);

    // Paste Special override from the Home/Formulas tabs: 0 = all,
    // 1 = formulas only, 2 = numeric values only.
    void setPasteMode(int m) { pasteMode_ = m; }

    // Direct cell editing
    bool isEditing() const { return editing; }
    void startEditing(const juce::String& initial);
    void commitEdit();
    void cancelEdit();

    // Perform a sheet mutation through the undo path.
    void performEdit(const std::function<void()>& change) { edit(change); }
    void selectCell(int newRow, int newCol, bool extend = false) { select(newRow, newCol, extend); }
    std::pair<int, int> selRows() const; // normalized selection rectangle
    std::pair<int, int> selCols() const;

    // --- View flags / zoom / autofit ---------------------------------------
    bool showGridlines = true;
    bool showRowNumbers = true;
    bool showColumnHeaders = true;
    bool freezeTopRow = false;
    juce::Colour playheadColour { 0xffe0b050 };
    float zoom = 1.0f;
    std::vector<int> customColWidths; // per-column pixel overrides (from autofit)

    void setShowGridlines(bool v) { showGridlines = v; repaint(); }
    void setShowRowNumbers(bool v) { showRowNumbers = v; repaint(); }
    void setShowColumnHeaders(bool v) { showColumnHeaders = v; repaint(); }
    void setFreezeTopRow(bool v) { freezeTopRow = v; if (v) scrollRow = 0; repaint(); }
    void setPlayheadColour(juce::Colour c) { playheadColour = c; repaint(); }
    void setZoom(float z) { zoom = juce::jlimit(0.5f, 3.0f, z); repaint(); }
    float getZoom() const { return zoom; }
    void autoFitColumnWidths();

    int headerH() const { return showColumnHeaders ? 34 : 0; }
    int stripW() const { return showRowNumbers ? 34 : 0; }
    int rowH() const { return std::max(8, static_cast<int>(22.0f * zoom)); }
    int colX(int c, int width, int numCols) const;
    int colW(int c, int width, int numCols) const;

private:
    struct Preview {
        juce::String text;
        bool isDefault = false; // no formula/value: showing the fallback
        bool isError = false;
        bool isRandom = false;
    };

    void timerCallback() override;
    void refresh();
    void select(int newRow, int newCol, bool extend = false);
    void ensureVisible();
    int numActiveSteps() const;
    int visibleRows() const;
    bool cellAt(juce::Point<float>, int& r, int& c) const;
    void headerAt(juce::Point<float>, int& c, bool& isOnColumn) const;

    bool isActiveCell() const;           // selection is a single cell

    std::string* formulaRef(int r, int c);
    void edit(const std::function<void()>& change); // snapshot for undo, apply, recompile
    void toggleStep(int r);

    MidisheetAudioProcessor& proc;
    int row = 0, col = 0, scrollRow = 0; // active cell
    int anchorRow = 0, anchorCol = 0;
    int selRowEnd = 0, selColEnd = 0; // other corner of the selection
    int lastStripSel = 0; // 1 = last selection was via row strip, 2 = via header
    int seenVersion = -1, seenStep = -2;
    juce::String clipboard; // TSV: formula-or-value per cell

    // Column drag-to-reorder state.
    bool colDragActive = false;
    int colDragFrom = -1; // source data column
    int colDragOver = -1; // drop target data column
    juce::Point<float> dragStart;

    // Cell value scrub (vertical drag over a cell adjusts its value).
    bool scrubActive = false;
    bool scrubUndoTaken = false;
    int scrubRow = -1, scrubCol = -1;
    double scrubStartValue = 0.0;

    juce::String currentHoverStatus; // updated by mouseMove/mouseExit
    int pasteMode_ = 0;

    // Direct cell editing state
    bool editing = false;
    juce::String editBuffer;
    juce::String originalFormula;

    // Preview per step per column (only visible columns are stored).
    std::vector<std::vector<Preview>> preview; // [row][col]
    std::vector<arp::Sheet> undoStack, redoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackerGrid)
};
