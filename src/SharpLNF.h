#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace juce {
// Sharp-cornered, Iosevka Charon Mono everywhere.
class SharpMonoLookAndFeel final : public LookAndFeel_V4
{
public:
    SharpMonoLookAndFeel()
    {
        setColour(ComboBox::arrowColourId, juce::Colour(0xffd6dbe2));
    }

    static juce::Font mono(float size = 14.0f)
    {
        return juce::Font(juce::FontOptions("Iosevka Charon Mono", size, juce::Font::bold));
    }

    Font getTextButtonFont(TextButton&, int buttonHeight) override
    {
        return mono(jmin(14.0f, (float) buttonHeight * 0.6f));
    }

    Font getComboBoxFont(ComboBox& box) override
    {
        return box.getText() == "Empty" ? mono().italicised() : mono();
    }

    Font getAlertWindowFont() override { return mono(14.0f); }
    Font getAlertWindowTitleFont() override { return mono(15.0f).boldened(); }
    Font getAlertWindowMessageFont() override { return mono(14.0f); }

    Font getPopupMenuFont() override { return popupItemItalic ? mono(14.0f).italicised() : mono(14.0f); }

    void drawPopupMenuItem(Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked, bool hasSubMenu, const String& text,
                           const String& shortcutKeyText, const Drawable* icon, const Colour* textColour) override
    {
        popupItemItalic = (text == "Empty");
        LookAndFeel_V4::drawPopupMenuItem(g, area, isSeparator, isActive, isHighlighted, isTicked, hasSubMenu,
                                          text, shortcutKeyText, icon, textColour);
    }

    void drawButtonBackground(Graphics& g, Button& button, const Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto baseColour = backgroundColour.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 0.9f)
                                          .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);
        if (isButtonDown || isMouseOverButton)
            baseColour = baseColour.contrasting(isButtonDown ? 0.2f : 0.05f);

        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(baseColour);
        g.fillRect(bounds);
        g.setColour(juce::Colour(0xff454c5a));
        g.drawRect(bounds, 1.0f);
    }

    void drawComboBox(Graphics& g, int width, int height, bool /*isButtonDown*/,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      ComboBox& box) override
    {
        auto bounds = box.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(box.findColour(ComboBox::backgroundColourId));
        g.fillRect(bounds);
        g.setColour(box.findColour(ComboBox::outlineColourId));
        g.drawRect(bounds, 1.0f);

        // equilateral triangle caret, centered in the drop-down button strip
        auto arrowRect = box.getLocalBounds().removeFromRight(14).toFloat().reduced(2.0f);
        const float triH = arrowRect.getHeight() * 0.38f;
        const float triW = triH * 2.0f / std::sqrt(3.0f);
        const float cx = arrowRect.getCentreX();
        const float topY = arrowRect.getCentreY() - triH * 0.5f;
        juce::Path p;
        p.startNewSubPath(cx - triW * 0.5f, topY);
        p.lineTo(cx + triW * 0.5f, topY);
        p.lineTo(cx, topY + triH);
        p.closeSubPath();
        g.setColour(box.findColour(ComboBox::arrowColourId));
        g.fillPath(p);
    }

    void drawToggleButton(Graphics& g, ToggleButton& button,
                          bool, bool) override
    {
        const float tickWidth = 12.0f;
        auto tickBounds = juce::Rectangle<float>(4.0f, ((float) button.getHeight() - tickWidth) * 0.5f, tickWidth, tickWidth);
        g.setColour(button.findColour(juce::ToggleButton::tickColourId));
        g.drawRect(tickBounds, 1.0f);
        if (button.getToggleState()) {
            g.fillRect(tickBounds.reduced(2.0f));
        }
        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(mono());
        g.drawText(button.getButtonText(), (int) tickWidth + 8, 0, button.getWidth() - (int) tickWidth - 8, button.getHeight(),
                   juce::Justification::centredLeft, true);
    }

    void drawTextEditorOutline(Graphics& g, int width, int height, TextEditor& editor) override
    {
        if (!editor.isEnabled())
            return;
        g.setColour(editor.findColour(TextEditor::outlineColourId));
        g.drawRect(0, 0, width, height, 1);
    }

    void drawAlertBox(Graphics& g, AlertWindow& alert, const Rectangle<int>&, TextLayout& textToDraw) override
    {
        const auto bg = alert.findColour(AlertWindow::backgroundColourId);
        g.setColour(bg);
        g.fillRect(alert.getLocalBounds());
        g.setColour(bg.contrasting(0.3f));
        g.drawRect(alert.getLocalBounds(), 1);

        auto area = alert.getLocalBounds().reduced(10);

        // AlertWindow reserves an 80px gutter on the left when an icon is shown.
        if (alert.getAlertType() != MessageBoxIconType::NoIcon)
            drawAlertIcon(g, alert, area.removeFromLeft(80));

        // The OK/Cancel row sits at 95% of the window height: keep it clear.
        int buttonTop = alert.getLocalBounds().getBottom();
        for (int i = alert.getNumButtons(); --i >= 0;)
            if (auto* b = alert.getButton(i))
                buttonTop = jmin(buttonTop, b->getY());
        area.removeFromBottom(jmax(0, alert.getLocalBounds().getBottom() - buttonTop + 10));

        // AlertWindow::updateLayout packs the title and the message into one
        // TextLayout - painting it is what actually shows the prompt's text.
        g.setColour(alert.findColour(AlertWindow::textColourId));
        textToDraw.draw(g, area.toFloat());
    }

    void drawAlertIcon(Graphics& g, const AlertWindow& alert, Rectangle<int> area)
    {
        if (alert.getAlertType() != MessageBoxIconType::WarningIcon)
            return;

        const float cx = (float) area.getCentreX();
        const float top = (float) area.getY() + 6.0f;
        const float halfW = 20.0f;
        const float triH = 36.0f;

        Path tri;
        tri.startNewSubPath(cx, top);
        tri.lineTo(cx + halfW, top + triH);
        tri.lineTo(cx - halfW, top + triH);
        tri.closeSubPath();

        g.setColour(Colour(0xffe0b050));
        g.fillPath(tri);

        g.setColour(alert.findColour(AlertWindow::backgroundColourId));
        g.setFont(mono(15.0f));
        g.drawText("!",
                   Rectangle<int>((int) cx - 8, (int) (top + triH * 0.34f), 16, (int) (triH * 0.62f)),
                   Justification::centred, true);
    }

private:
    bool popupItemItalic = false;
};
} // namespace juce
