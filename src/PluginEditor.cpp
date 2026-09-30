#include "PluginEditor.h"

namespace {

const char* const kHint = "hjkl move  i/Enter edit  = or digit: new formula  x clear  space on/off  y/p copy/paste  u undo";

} // namespace

ArpExcelAudioProcessorEditor::ArpExcelAudioProcessorEditor(ArpExcelAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), grid(p)
{
    auto& apvts = p.getAPVTS();

    enabledButton.setButtonText("Enabled");
    modeCombo.addItemList(apvts.getParameter("mode")->getAllValueStrings(), 1);
    rateCombo.addItemList(apvts.getParameter("rate")->getAllValueStrings(), 1);
    for (auto* c : std::initializer_list<juce::Component*>{&enabledButton, &modeCombo, &rateCombo}) {
        c->setWantsKeyboardFocus(false); // keep keys going to the grid
        addAndMakeVisible(*c);
    }

    const std::pair<juce::Slider*, juce::Label*> sliders[] = {
        {&numStepsSlider, &numStepsLabel}, {&gateSlider, &gateLabel},
        {&octaveRangeSlider, &octaveRangeLabel}, {&swingSlider, &swingLabel}};
    const char* const names[] = {"Steps", "Gate", "Octaves", "Swing"};
    for (size_t i = 0; i < std::size(sliders); ++i) {
        auto [slider, label] = sliders[i];
        slider->setSliderStyle(juce::Slider::LinearBar);
        slider->setWantsKeyboardFocus(false);
        label->setText(names[i], juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(*slider);
        addAndMakeVisible(*label);
    }

    enabledAttachment = std::make_unique<ButtonAttachment>(apvts, "enabled", enabledButton);
    modeAttachment = std::make_unique<ComboBoxAttachment>(apvts, "mode", modeCombo);
    rateAttachment = std::make_unique<ComboBoxAttachment>(apvts, "rate", rateCombo);
    numStepsAttachment = std::make_unique<SliderAttachment>(apvts, "numSteps", numStepsSlider);
    gateAttachment = std::make_unique<SliderAttachment>(apvts, "gate", gateSlider);
    octaveRangeAttachment = std::make_unique<SliderAttachment>(apvts, "octaveRange", octaveRangeSlider);
    swingAttachment = std::make_unique<SliderAttachment>(apvts, "swing", swingSlider);
    for (auto* s : {&gateSlider, &swingSlider}) {
        s->textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; };
        s->valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() / 100.0; };
        s->updateText();
    }

    // Formula bar
    const auto mono = juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain);
    cellLabel.setFont(juce::Font(mono));
    cellLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0b050));
    formulaBar.setFont(juce::Font(mono));
    formulaBar.setTextToShowWhenEmpty("(empty: uses the default)", juce::Colour(0xff646c78));
    formulaBar.onReturnKey = [this] { commitFormula(); grid.grabKeyboardFocus(); };
    formulaBar.onEscapeKey = [this] { showSelectedCell(); grid.grabKeyboardFocus(); };
    formulaBar.onFocusLost = [this] { commitFormula(); };
    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(cellLabel);
    addAndMakeVisible(formulaBar);
    addAndMakeVisible(statusLabel);

    grid.onSelectionChanged = [this] { showSelectedCell(); };
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
}

void ArpExcelAudioProcessorEditor::visibilityChanged()
{
    // Focus can only be taken once the window is on screen.
    if (isShowing() && !formulaBar.hasKeyboardFocus(true))
        grid.grabKeyboardFocus();
}

void ArpExcelAudioProcessorEditor::parentHierarchyChanged()
{
    visibilityChanged();
}

ArpExcelAudioProcessorEditor::~ArpExcelAudioProcessorEditor() {}

void ArpExcelAudioProcessorEditor::showSelectedCell()
{
    cellLabel.setText(grid.cellName(), juce::dontSendNotification);
    if (!formulaBar.hasKeyboardFocus(true))
        formulaBar.setText(grid.cellFormula(), false);

    const auto err = grid.cellError();
    statusLabel.setText(err.isEmpty() ? juce::String(kHint) : "Error: " + err, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId,
                          err.isEmpty() ? juce::Colour(0xff646c78) : juce::Colour(0xffff7a59));
}

void ArpExcelAudioProcessorEditor::commitFormula()
{
    grid.setCellFormula(formulaBar.getText());
    showSelectedCell();
}

void ArpExcelAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff111317));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("ArpExcel", 12, 8, 120, 28, juce::Justification::centredLeft);
}

void ArpExcelAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(10);

    auto row1 = area.removeFromTop(28);
    row1.removeFromLeft(120); // title
    enabledButton.setBounds(row1.removeFromLeft(90));
    row1.removeFromLeft(8);
    modeCombo.setBounds(row1.removeFromLeft(120));
    row1.removeFromLeft(8);
    rateCombo.setBounds(row1.removeFromLeft(80));
    area.removeFromTop(6);

    auto row2 = area.removeFromTop(24);
    const int each = row2.getWidth() / 4;
    const std::pair<juce::Slider*, juce::Label*> sliders[] = {
        {&numStepsSlider, &numStepsLabel}, {&gateSlider, &gateLabel},
        {&octaveRangeSlider, &octaveRangeLabel}, {&swingSlider, &swingLabel}};
    for (auto [slider, label] : sliders) {
        auto cell = row2.removeFromLeft(each).reduced(2, 0);
        label->setBounds(cell.removeFromLeft(60));
        slider->setBounds(cell.reduced(4, 0));
    }
    area.removeFromTop(8);

    auto bar = area.removeFromTop(26);
    cellLabel.setBounds(bar.removeFromLeft(70));
    formulaBar.setBounds(bar);
    area.removeFromTop(4);

    statusLabel.setBounds(area.removeFromBottom(20));
    area.removeFromBottom(4);
    grid.setBounds(area);
}
