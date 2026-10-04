#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace {

// Step length in quarter notes, matching the "rate" choices below.
constexpr double kRates[] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625,
                             4.0 * 2 / 3, 2.0 * 2 / 3, 1.0 * 2 / 3, 0.5 * 2 / 3,
                             0.25 * 2 / 3, 0.125 * 2 / 3, 0.0625 * 2 / 3};

} // namespace

// AU MIDI FX (Logic) must have no audio buses or auval fails. VST3 has no
// MIDI-effect category, so hosts like Ableton load it as an instrument and
// refuse it without an audio output: give VST3 a silent stereo output.
juce::AudioProcessor::BusesProperties MidisheetAudioProcessor::makeBuses() {
    if (juce::PluginHostType::getPluginLoadedAs() == wrapperType_VST3)
        return BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
    return {};
}

MidisheetAudioProcessor::MidisheetAudioProcessor()
    : AudioProcessor(makeBuses()),
      apvts_(*this, nullptr, "Parameters", createParameterLayout())
{
    sheetChanged();
}

MidisheetAudioProcessor::~MidisheetAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout MidisheetAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"enabled", 1}, "Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1}, "Mode",
        juce::StringArray{"Up", "Down", "Up-Down", "Down-Up", "Random", "Order", "Chord"},
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"rate", 1}, "Rate",
        juce::StringArray{"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                          "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T", "1/64T"},
        4));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gate", 1}, "Gate", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"octaveRange", 1}, "Octave Range", 1, 8, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"swing", 1}, "Swing", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"length", 1}, "Length", juce::NormalisableRange<float>(0.1f, 16.0f), 1.0f));

    return {params.begin(), params.end()};
}

juce::StringArray MidisheetAudioProcessor::sheetChanged() {
    sheetErrors_.clear();
    exchange_.publish(sheet_.compile(sheetErrors_));
    ++sheetVersion_;
    return sheetErrors_;
}

void MidisheetAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate);
    events_.clear();
    events_.reserve(static_cast<size_t>(std::max(8192, samplesPerBlock * 4)));
    outBuffer_.ensureSize(4096);
}

void MidisheetAudioProcessor::releaseResources() {}

void MidisheetAudioProcessor::syncSettings() {
    auto& s = engine_.settings;
    auto raw = [this](const char* id) { return apvts_.getRawParameterValue(id)->load(); };
    s.enabled = raw("enabled") > 0.5f;
    s.mode = static_cast<arp::Mode>(static_cast<int>(raw("mode")));
    s.rateBeats = kRates[juce::jlimit(0, 13, static_cast<int>(raw("rate")))];
    s.gate = raw("gate");
    s.octaves = static_cast<int>(raw("octaveRange"));
    s.swing = raw("swing");
    s.length = raw("length");
}

namespace {

// Velocity curves for the input gain stage of the arp.
int curveVelocity(int v, int idx)
{
    switch (idx) {
        case 1: return juce::jlimit(0, 127, static_cast<int>(std::sqrt(v / 127.0) * 127.0)); // Soft
        case 2: return juce::jlimit(0, 127, static_cast<int>((v / 127.0) * (v / 127.0) * 127.0)); // Hard
        case 3: return v > 0 ? 127 : 0;
        case 0:
        default: return v;
    }
}

} // namespace

void MidisheetAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
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
        bool isNote = false;
        if (msg.isNoteOn() || msg.isNoteOff()) {
            isNote = true;
            const int ch = msg.getChannel();
            const int n = msg.getNoteNumber();
            const bool inFilter = (midiInputChannel.load() == 0 || midiInputChannel.load() == ch) &&
                                  n >= midiMinNote.load() && n <= midiMaxNote.load();
            if (enabled && inFilter) {
                if (msg.isNoteOn())
                    engine_.noteOn(ch, n, curveVelocity(msg.getVelocity(), midiCurve.load()));
                else
                    engine_.noteOff(ch, n);
            }
            if (!enabled || midiThru.load() || midiRouting.load() == 1)
                out.addEvent(msg, metadata.samplePosition);
        } else {
            if (msg.isAllNotesOff() || msg.isAllSoundOff())
                engine_.allNotesOff();
            out.addEvent(msg, metadata.samplePosition);
        }
    }

    events_.clear();
    engine_.process(transport, exchange_.acquire(), buffer.getNumSamples(), events_);
    playingStep_.store(engine_.lastPatternStep(), std::memory_order_relaxed);
    const int outCh = juce::jlimit(1, 16, midiOutputChannel.load());
    for (const auto& e : events_) {
        if (e.isCC) {
            out.addEvent(juce::MidiMessage::controllerEvent(outCh, e.ccNumber, e.ccValue),
                         e.sampleOffset);
            continue;
        }
        out.addEvent(e.noteOn ? juce::MidiMessage::noteOn(outCh, e.pitch, static_cast<juce::uint8>(e.velocity))
                              : juce::MidiMessage::noteOff(outCh, e.pitch),
                     e.sampleOffset);
    }
    midiMessages.swapWith(out);
}

juce::AudioProcessorEditor* MidisheetAudioProcessor::createEditor() {
    return new MidisheetAudioProcessorEditor(*this);
}

void MidisheetAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    state.setProperty("sheet", juce::JSON::toString(sheet_.toVar(), true), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void MidisheetAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xmlState = getXmlFromBinary(data, sizeInBytes);
    if (xmlState == nullptr || !xmlState->hasTagName(apvts_.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml(*xmlState);
    if (state.hasProperty("sheet")) {
        sheet_.fromVar(juce::JSON::parse(state["sheet"].toString()));
        state.removeProperty("sheet", nullptr);
        sheetChanged();
    }
    apvts_.replaceState(state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MidisheetAudioProcessor();
}
