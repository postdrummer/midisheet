#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {

// Step length in quarter notes, matching the "rate" choices below.
constexpr double kRates[] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625};

} // namespace

// AU MIDI FX (Logic) must have no audio buses or auval fails. VST3 has no
// MIDI-effect category, so hosts like Ableton load it as an instrument and
// refuse it without an audio output: give VST3 a silent stereo output.
juce::AudioProcessor::BusesProperties ArpExcelAudioProcessor::makeBuses() {
    if (juce::PluginHostType::getPluginLoadedAs() == wrapperType_VST3)
        return BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
    return {};
}

ArpExcelAudioProcessor::ArpExcelAudioProcessor()
    : AudioProcessor(makeBuses()),
      apvts_(*this, nullptr, "Parameters", createParameterLayout())
{
    patternChanged();
}

ArpExcelAudioProcessor::~ArpExcelAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout ArpExcelAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"enabled", 1}, "Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1}, "Mode",
        juce::StringArray{"Up", "Down", "Up-Down", "Down-Up", "Random", "Order", "Chord"},
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"rate", 1}, "Rate",
        juce::StringArray{"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"},
        4));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"numSteps", 1}, "Num Steps", 1, arp::kMaxSteps, 16));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gate", 1}, "Gate", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"octaveRange", 1}, "Octave Range", 1, 8, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"swing", 1}, "Swing", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    return {params.begin(), params.end()};
}

juce::StringArray ArpExcelAudioProcessor::patternChanged() {
    patternErrors_.clear();
    exchange_.publish(pattern_.compile(patternErrors_));
    ++patternVersion_;
    return patternErrors_;
}

void ArpExcelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate);
    events_.clear();
    events_.reserve(static_cast<size_t>(std::max(1024, samplesPerBlock)));
    outBuffer_.ensureSize(4096);
}

void ArpExcelAudioProcessor::releaseResources() {}

void ArpExcelAudioProcessor::syncSettings() {
    auto& s = engine_.settings;
    auto raw = [this](const char* id) { return apvts_.getRawParameterValue(id)->load(); };
    s.enabled = raw("enabled") > 0.5f;
    s.mode = static_cast<arp::Mode>(static_cast<int>(raw("mode")));
    s.rateBeats = kRates[juce::jlimit(0, 6, static_cast<int>(raw("rate")))];
    s.numSteps = static_cast<int>(raw("numSteps"));
    s.gate = raw("gate");
    s.octaves = static_cast<int>(raw("octaveRange"));
    s.swing = raw("swing");
}

void ArpExcelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    syncSettings();

    arp::Transport transport;
    if (auto* playHead = getPlayHead()) {
        if (auto position = playHead->getPosition()) {
            transport.playing = position->getIsPlaying();
            transport.ppqStart = position->getPpqPosition().orFallback(0.0);
            transport.bpm = position->getBpm().orFallback(120.0);
        }
    }

    const bool enabled = engine_.settings.enabled;
    auto& out = outBuffer_;
    out.clear();
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine_.noteOn(msg.getChannel(), msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            engine_.noteOff(msg.getChannel(), msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine_.allNotesOff();

        // The arp replaces the played notes; everything else passes through.
        // When bypassed, pass the played notes too.
        if (!enabled || !(msg.isNoteOn() || msg.isNoteOff()))
            out.addEvent(msg, metadata.samplePosition);
    }

    events_.clear();
    engine_.process(transport, exchange_.acquire(), buffer.getNumSamples(), events_);
    playingStep_.store(engine_.lastPatternStep(), std::memory_order_relaxed);
    for (const auto& e : events_) {
        out.addEvent(e.noteOn ? juce::MidiMessage::noteOn(e.channel, e.pitch, static_cast<juce::uint8>(e.velocity))
                              : juce::MidiMessage::noteOff(e.channel, e.pitch),
                     e.sampleOffset);
    }
    midiMessages.swapWith(out);
}

juce::AudioProcessorEditor* ArpExcelAudioProcessor::createEditor() {
    return new ArpExcelAudioProcessorEditor(*this);
}

void ArpExcelAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    state.setProperty("pattern", juce::JSON::toString(pattern_.toVar(), true), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void ArpExcelAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xmlState = getXmlFromBinary(data, sizeInBytes);
    if (xmlState == nullptr || !xmlState->hasTagName(apvts_.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml(*xmlState);
    if (state.hasProperty("pattern")) {
        pattern_.fromVar(juce::JSON::parse(state["pattern"].toString()));
        state.removeProperty("pattern", nullptr);
        patternChanged();
    }
    apvts_.replaceState(state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ArpExcelAudioProcessor();
}
