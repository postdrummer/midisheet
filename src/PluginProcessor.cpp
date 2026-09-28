#include "PluginProcessor.h"
#include "PluginEditor.h"

ArpExcelAudioProcessor::ArpExcelAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout())
{
}

ArpExcelAudioProcessor::~ArpExcelAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout ArpExcelAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "enabled", "Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Mode",
        juce::StringArray{"Up", "Down", "Up-Down", "Down-Up", "Random", "Order", "Chord"},
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "rate", "Rate",
        juce::StringArray{"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"},
        4));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "numSteps", "Num Steps", 1, 64, 16));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "gate", "Gate", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "octaveRange", "Octave Range", 1, 8, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "swing", "Swing", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    return {params.begin(), params.end()};
}

void ArpExcelAudioProcessor::prepareToPlay(double sampleRate, int) {
    arpeggiator_.getFormulaEngine().setVariable("SAMPLERATE", sampleRate);
}

void ArpExcelAudioProcessor::releaseResources() {}

void ArpExcelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    arpeggiator_.setEnabled(apvts_.getRawParameterValue("enabled")->load() > 0.5f);
    arpeggiator_.setMode(static_cast<arp::ArpMode>(static_cast<int>(apvts_.getRawParameterValue("mode")->load())));
    arpeggiator_.setRate(static_cast<arp::ArpRate>(static_cast<int>(apvts_.getRawParameterValue("rate")->load())));
    arpeggiator_.setNumSteps(static_cast<int>(apvts_.getRawParameterValue("numSteps")->load()));
    arpeggiator_.setGatePercent(apvts_.getRawParameterValue("gate")->load());
    arpeggiator_.setOctaveRange(static_cast<int>(apvts_.getRawParameterValue("octaveRange")->load()));
    arpeggiator_.setSwing(apvts_.getRawParameterValue("swing")->load());

    double bpm = 120.0;
    if (auto* playHead = getPlayHead()) {
        if (auto position = playHead->getPosition()) {
            if (auto bpmOpt = position->getBpm()) {
                bpm = *bpmOpt;
            }
        }
    }

    arpeggiator_.process(midiMessages, buffer.getNumSamples(), bpm);
}

juce::AudioProcessorEditor* ArpExcelAudioProcessor::createEditor() {
    return new ArpExcelAudioProcessorEditor(*this);
}

void ArpExcelAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ArpExcelAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(apvts_.state.getType())) {
            apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ArpExcelAudioProcessor();
}
