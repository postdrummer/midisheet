#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "../src/PluginProcessor.h"
#include "../src/TrackerGrid.h"

// A ComboBox that supports an on-track "click drag to change value" gesture:
// drag vertically while held to cycle the selection; a clean /"click-by-
// touch-release" still opens the regular pop-up list on mouse up.
class DragCombo final : public juce::ComboBox
{
public:
    void mouseDown(const juce::MouseEvent& e) override
    {
        y0_ = e.position.y;
        idx0_ = getSelectedItemIndex();
        dragged_ = false;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragged_ && std::abs(e.position.y - y0_) > 5.0f)
            dragged_ = true;
        if (dragged_) {
            const float delta = (e.position.y - y0_) / 12.0f;
            const int indexDelta = delta < 0 ? -static_cast<int>(std::abs(delta) + 0.5f)
                                             : static_cast<int>(delta + 0.5f);
            const int idx = juce::jlimit(0, getNumItems() - 1, idx0_ + indexDelta);
            if (idx != getSelectedItemIndex())
                setSelectedItemIndex(idx, juce::sendNotification);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!dragged_) {
            juce::ComboBox::mouseDown(e);
            juce::ComboBox::mouseUp(e);
        }
    }

private:
    float y0_ = 0.0f;
    int idx0_ = -1;
    bool dragged_ = false;
};


// Top panel replacing the ribbon: File | Defaults | Edit groups over an
// info-bar that briefly explains whatever control is hovered.
class TopPanel final : public juce::Component, private juce::Timer
{
public:
    TopPanel(MidisheetAudioProcessor& proc, TrackerGrid& grid, juce::Label& infoBar);
    ~TopPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshFromSheet(); // re-reads defaults + active column type
    void pickFile(bool save);

private:
    void timerCallback() override;
    void setInfo(const juce::String& text);
    void watchHover(juce::Component& c, const juce::String& info);

    struct TypeRow {
        arp::ColumnType type;
        juce::String label;
        std::unique_ptr<juce::Label> labelComp;
        std::unique_ptr<juce::TextEditor> text; // numeric text box
        std::unique_ptr<juce::ComboBox> combo;  // dropdown variant
    };

    void buildDefaultsRow(TypeRow& row, int x, int y, int w);
    void applyDefault(arp::ColumnType type, double value);
    void refreshDefaultsRow(TypeRow& row) const;

    MidisheetAudioProcessor& proc;
    TrackerGrid& grid;
    juce::Label& infoBar;

    // --- groups ---
    juce::Label fileTitle, defaultsTitle, editTitle;
    // File
    juce::TextButton restoreBtn, loadBtn, saveBtn, saveAsBtn;
    // Defaults (two columns of rows)
    std::vector<TypeRow> defaultRows;
    std::unique_ptr<juce::FileChooser> fileChooser_;
    // Edit
    juce::Label addLabel_ { {}, "Add:" };
    juce::TextEditor addName;
    DragCombo addType, datatypeCombo;
    juce::Label dtLabel_ { {}, "Datatype:" };
    juce::TextButton addBtn, deleteBtn, mergeBtn;

    int seenSheetVersion = -1;
    std::vector<std::unique_ptr<juce::MouseListener>> hoverListeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopPanel)
};
