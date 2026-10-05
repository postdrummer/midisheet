#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "TrackerGrid.h"
#include "Ribbon.h"
#include "TopPanel.h"

// Formula bar TextEditor that yields nav keys to the suggestions popup when
// it is visible (arrows, Enter/Tab to accept, Escape to dismiss).
class FormulaEditor final : public juce::TextEditor
{
public:
    std::function<bool()> acVisibleHook;
    std::function<void()> acUp, acDown, acAccept, acDismiss;

    bool keyPressed(const juce::KeyPress& k) override
    {
        const bool showing = acVisibleHook != nullptr && acVisibleHook();
        if (showing) {
            const int code = k.getKeyCode();
            if (code == juce::KeyPress::upKey) { if (acUp) acUp(); return true; }
            if (code == juce::KeyPress::downKey) { if (acDown) acDown(); return true; }
            if (code == juce::KeyPress::returnKey && acAccept) { acAccept(); return true; }
            if (code == juce::KeyPress::tabKey && acAccept) { acAccept(); return true; }
            if (code == juce::KeyPress::escapeKey && acDismiss) { acDismiss(); return true; }
        }
        return juce::TextEditor::keyPressed(k);
    }
};

// Dropdown with completion candidates driven by a juce::ListBoxModel.
struct SuggestionsBox final : public juce::ListBox, public juce::ListBoxModel
{
    SuggestionsBox() : juce::ListBox({}, this) { setWantsKeyboardFocus(false); setRowHeight(22); }

    std::function<void(juce::String)> onPick;
    juce::StringArray items;

    int getNumRows() override { return items.size(); }
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool selected) override
    {
        g.fillAll(selected ? juce::Colour(0xffe0b050) : juce::Colour(0xff1e2127));
        g.setColour(selected ? juce::Colours::black : juce::Colour(0xffe6e9ee));
        g.setFont(juce::Font(juce::FontOptions("Iosevka Charon Mono", 15.0f, juce::Font::bold)));
        g.drawText(items[rowNumber], 4, 0, width - 8, height, juce::Justification::centredLeft);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (onPick && row >= 0 && row < items.size())
            onPick(items[row]);
    }
};

class MidisheetAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MidisheetAudioProcessorEditor(MidisheetAudioProcessor&);
    ~MidisheetAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    void showSelectedCell();
    void commitFormula();
    void updateStatus();

    MidisheetAudioProcessor& processorRef;

    // Ribbon hover text; refreshed by the focus sampler tick and reported
    // through the status bar.
    juce::String ribbonHover;

    // Top-of-editor controls (enabled/mode/rate + step sliders) were removed
    // for now. The APVTS parameters remain in the host layout and the engine
    // still reads them in syncSettings(), so automation/defaults still work.

    juce::TextEditor nameBox;      // e.g. "C3" or "A1:C5"; typing a ref jumps
    FormulaEditor formulaBar;      // formula of the selected cell
    juce::Label statusLabel;       // compile error / contextual info / Ready
    TrackerGrid grid;
    juce::TooltipWindow tooltipWindow { this, 300 }; // enables setTooltip() below
    Ribbon ribbon;
    TopPanel topPanel { processorRef, grid, statusLabel };

    SuggestionsBox acBox;

private:
    void hideSuggestions();
    void updateSuggestions();
    void acceptSuggestion(juce::String item = {});
    void navSuggestions(int dir);
    juce::StringArray candidatesForCompletion();

    void timerCallback() override;
    void fitHeightToRows();
    int seenVersion = -1;
    float seenZoom = -1.0f;

public:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidisheetAudioProcessorEditor)
};
