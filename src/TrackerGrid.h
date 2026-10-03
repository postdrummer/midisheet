#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

#include <array>
#include <functional>
#include <vector>

/**
 * Spreadsheet grid: one row per step, one column per sheet column.
 *
 * Column headers show the data type (top line) and column name (bottom line).
 * Cells show the evaluated value for a preview chord (C E G, held at velocity
 * 100, in Up order); the formula behind the selected cell is shown in the
 * editor's formula bar.
 *
 * Keys (vim + spreadsheet):
 *   h j k l / arrows   move            gg / G        first / last step
 *   Ctrl-d / Ctrl-u    half page       Tab / S-Tab   next / previous column
 *   i  a  Enter  F2    edit formula    = 0-9 - .     start typing a new formula
 *   x  Delete  Bksp    clear cell      Space         toggle step on/off
 *   y / p              copy / paste    u / Ctrl-r    undo / redo (Cmd-Z works too)
 * Mouse: click to select, double-click to edit, click the "on" column to toggle.
 */
class TrackerGrid : public juce::Component, private juce::Timer {
public:
    explicit TrackerGrid(MidisheetAudioProcessor&);

    int getRow() const { return row; }
    int getCol() const { return col; }
    juce::String cellName() const;               // e.g. "C3"
    juce::String cellFormula() const;            // formula text of the selected cell
    juce::String cellError() const;              // compile error of the selected cell, if any
    void setCellFormula(const juce::String& text); // commits (with undo)

    std::function<void()> onSelectionChanged;
    std::function<void(const juce::String& initialText)> onEditRequested;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Preview {
        juce::String text;
        bool isDefault = false; // no formula/value: showing the fallback
        bool isError = false;
        bool isRandom = false;
    };

    void timerCallback() override;
    void refresh();
    void select(int newRow, int newCol);
    void ensureVisible();
    int numActiveSteps() const;
    int visibleRows() const;
    bool cellAt(juce::Point<int>, int& r, int& c) const;

    std::string* formulaRef(int r, int c);
    void edit(const std::function<void()>& change); // snapshot for undo, apply, recompile
    void toggleStep(int r);
    void undo();
    void redo();

    MidisheetAudioProcessor& proc;
    int row = 0, col = 0, scrollRow = 0;
    int seenVersion = -1, seenStep = -2;
    bool pendingG = false;
    juce::String clipboard;

    // Preview per step per column (only visible columns are stored).
    std::vector<std::vector<Preview>> preview; // [row][col]
    std::vector<arp::Sheet> undoStack, redoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackerGrid)
};
