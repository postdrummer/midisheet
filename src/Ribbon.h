#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "TrackerGrid.h"

#include <functional>
#include <memory>
#include <vector>

// A compact numeric field; drag up/down to change the value.
// Shift = x10 step, Alt/Option = x0.1 step.
class DragField final : public juce::Component
{
public:
    DragField(float initial, float low, float high, int decimals,
              std::function<void(float)> onChange)
        : value(initial), minValue(low), maxValue(high), decimals_(decimals), onChange_(std::move(onChange)) {}

    float getValue() const { return value; }
    void setValue(float v, bool notify = false)
    {
        value = juce::jlimit(minValue, maxValue, v);
        if (notify && onChange_)
            onChange_(value);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff1e2127));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1), 3.0f);
        g.setColour(juce::Colour(0xff333946));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 3.0f, 1.0f);
        g.setColour(juce::Colour(0xffe6e9ee));
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
        g.drawText(juce::String(value, decimals_), 0, 0, getWidth(), getHeight(), juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override { dragY = e.position.y; original = value; }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float dy = (dragY - e.position.y) / 40.0f;
        float step = (maxValue - minValue) / 20.0f;
        if (e.mods.isShiftDown()) step *= 10.0f;
        if (e.mods.isAltDown()) step /= 10.0f;
        setValue(original + dy * step, true);
    }

private:
    float value, minValue, maxValue;
    int decimals_;
    std::function<void(float)> onChange_;
    float original = 0.0f;
    float dragY = 0.0f;
};

// One tab page: a row of controls laid out left to right (wrapping).
class RibbonTab final : public juce::Component
{
public:
    void addItem(std::unique_ptr<juce::Component> c, int w)
    {
        addAndMakeVisible(*c);
        owned.push_back({std::move(c), w});
        resized();
    }

    // Adds a pointer to a component owned elsewhere (e.g. Ribbon member
    // unique_ptrs); not owned by the tab.
    void addExisting(juce::Component& c, int w)
    {
        addAndMakeVisible(c);
        existing.push_back({&c, w});
        resized();
    }

    void resized() override
    {
        int x = 4, y = 3, h = getHeight() - 6;
        for (auto& it : owned) {
            if (x + it.w > getWidth() - 4) { x = 4; y += h + 4; }
            it.c->setBounds(x, y, it.w, h);
            x += it.w + 4;
        }
        for (auto& it : existing) {
            if (x + it.w > getWidth() - 4) { x = 4; y += h + 4; }
            it.c->setBounds(x, y, it.w, h);
            x += it.w + 4;
        }
    }

private:
    struct Item { std::unique_ptr<juce::Component> c; int w; };
    struct XItem { juce::Component* c; int w; };
    std::vector<Item> owned;
    std::vector<XItem> existing;
};

class Ribbon final : public juce::Component
{
public:
    Ribbon(MidisheetAudioProcessor& p, TrackerGrid& grid, juce::TextEditor& formula);

    void activeColumnChanged(); // refresh per-column control states

    void resized() override;

private:
    void buildHome(RibbonTab&);
    void buildColumns(RibbonTab&);
    void buildArp(RibbonTab&);
    void buildFormulas(RibbonTab&);
    void buildView(RibbonTab&);
    void buildMidi(RibbonTab&);
    void buildFile(RibbonTab&);

    static void setParam(juce::AudioProcessorValueTreeState& apvts, const char* id, float normValue);
    static float getParamNorm(juce::AudioProcessorValueTreeState& apvts, const char* id);

    void handleSheetFile(const juce::File& file, bool save);
    void saveLoadFile(bool save);
    void exportCsv();
    void traceRefs(bool precedents);

    MidisheetAudioProcessor& proc;
    TrackerGrid& grid_;
    juce::TextEditor& formula_;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    // Persistent columns-tab controls (enabled state follows the active cell).
    std::unique_ptr<juce::ComboBox> colTypeCombo_;
    std::unique_ptr<juce::TextEditor> colNameEditor_;
    std::unique_ptr<juce::TextEditor> colDefaultEditor_;
    std::unique_ptr<juce::ToggleButton> colVisibleToggle_;

    // Lifetime holders for async choosers/popups.
    std::unique_ptr<juce::FileChooser> fileChooser_;
    std::unique_ptr<juce::PopupMenu> popup_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Ribbon)
};
