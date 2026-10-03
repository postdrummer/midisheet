#include "ArpEngine.h"

#include <algorithm>
#include <cmath>

namespace arp {

StepResult evaluateStep(const CompiledSheet& sheet, int patternStep, const StepInput& in, int prevNote,
                        double defaultGate, uint32_t* rng)
{
    formula::Context ctx;
    ctx.cells = sheet.cells.data();
    ctx.rng = rng;
    ctx.set(formula::Var::Step, patternStep);
    ctx.set(formula::Var::Note, in.pitch);
    ctx.set(formula::Var::Velocity, in.velocity);
    ctx.set(formula::Var::Channel, in.channel);
    ctx.set(formula::Var::Prev, prevNote);
    ctx.set(formula::Var::Length, 1.0);

    StepResult r;
    r.pitch = in.pitch;
    r.velocity = in.velocity;
    r.gate = defaultGate;
    r.length = 1.0;
    r.playable = true;

    // Evaluate visible columns left-to-right; each column transforms the signal.
    for (int col = 0; col < sheet.numCols; ++col) {
        const auto& meta = sheet.cols[static_cast<size_t>(col)];
        if (!meta.visible)
            continue;

        const double value = sheet.evaluateCell(col, patternStep, ctx);

        switch (meta.type) {
            case ColumnType::Note:
                // Negative = pass through the incoming note.
                if (value >= 0.0)
                    r.pitch = static_cast<int>(std::lround(value));
                break;
            case ColumnType::Shift:
                r.pitch += static_cast<int>(std::lround(value));
                break;
            case ColumnType::Octave:
                r.pitch += static_cast<int>(std::lround(value)) * 12;
                break;
            case ColumnType::Velocity:
                r.velocity = static_cast<int>(std::lround(value));
                break;
            case ColumnType::Gate:
                r.gate = value / 100.0; // gate columns are in percent
                break;
            case ColumnType::Length:
                r.length = value;
                ctx.set(formula::Var::Length, r.length);
                break;
            default:
                break; // Time, Chance, CC, Text, Formula: no direct effect yet
        }
    }

    r.pitch = std::clamp(r.pitch, 0, 127);
    r.velocity = std::clamp(r.velocity, 1, 127);
    r.gate = std::clamp(r.gate, 0.01, 1.0);
    r.length = std::clamp(r.length, 0.1, 16.0);
    r.playable = r.pitch >= 0 && r.pitch <= 127;
    return r;
}

ArpEngine::ArpEngine() = default;

void ArpEngine::prepare(double sr)
{
    sampleRate = sr;
    reset();
}

void ArpEngine::reset()
{
    numHeld = 0;
    numPending = 0;
    seqDirty = true;
    sequencePos = 0;
    freePpq = 0.0;
    lastPpqEnd = -1.0;
    wasPlaying = false;
}

void ArpEngine::noteOn(int channel, int pitch, int velocity)
{
    noteOff(channel, pitch);
    if (numHeld == 0)
        sequencePos = 0; // a fresh chord restarts the order
    if (numHeld < kMaxHeld)
        held[static_cast<size_t>(numHeld++)] = {channel, pitch, velocity, arrivalCounter++};
    seqDirty = true;
}

void ArpEngine::noteOff(int channel, int pitch)
{
    for (int i = 0; i < numHeld; ++i) {
        if (held[static_cast<size_t>(i)].pitch == pitch && held[static_cast<size_t>(i)].channel == channel) {
            std::move(held.begin() + i + 1, held.begin() + numHeld, held.begin() + i);
            --numHeld;
            seqDirty = true;
            return;
        }
    }
}

void ArpEngine::allNotesOff()
{
    numHeld = 0;
    seqDirty = true;
}

uint32_t ArpEngine::nextRandom()
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

void ArpEngine::buildSequence()
{
    builtMode = settings.mode;
    builtOctaves = std::clamp(settings.octaves, 1, 8);
    seqDirty = false;

    std::array<HeldNote, kMaxHeld> notes = held;
    auto end = notes.begin() + numHeld;
    if (settings.mode == Mode::Order)
        std::sort(notes.begin(), end, [](auto& a, auto& b) { return a.order < b.order; });
    else
        std::sort(notes.begin(), end, [](auto& a, auto& b) { return a.pitch < b.pitch; });

    seqLen = 0;
    for (int oct = 0; oct < builtOctaves; ++oct)
        for (auto it = notes.begin(); it != end; ++it)
            seq[static_cast<size_t>(seqLen++)] = {it->channel, it->pitch + 12 * oct, it->velocity};

    auto first = seq.begin(), last = seq.begin() + seqLen;
    if (settings.mode == Mode::Down || settings.mode == Mode::DownUp)
        std::reverse(first, last);
    if ((settings.mode == Mode::UpDown || settings.mode == Mode::DownUp) && seqLen > 2) {
        // 1 2 3 4 3 2 — endpoints not repeated
        for (int i = seqLen - 2; i > 0; --i)
            seq[static_cast<size_t>(seqLen + (seqLen - 1 - i) - 1)] = seq[static_cast<size_t>(i)];
        seqLen += seqLen - 2;
    }
}

void ArpEngine::playNote(const SeqNote& src, int patternStep, double stepPpq, int sampleOffset,
                          const CompiledSheet& sheet, std::vector<MidiOut>& out)
{
    const StepResult r = evaluateStep(sheet, patternStep, {src.channel, src.pitch, src.velocity}, prevNote,
                                      settings.gate, &rngState);
    if (!r.playable)
        return;
    const int pitch = r.pitch, velocity = r.velocity;
    const double gate = r.gate, length = r.length;

    // Retrigger: close a still-sounding copy of this pitch first.
    for (int i = 0; i < numPending;) {
        auto& p = pending[static_cast<size_t>(i)];
        if (p.pitch == pitch && p.channel == src.channel) {
            out.push_back({sampleOffset, false, p.channel, p.pitch, 0});
            p = pending[static_cast<size_t>(--numPending)];
        } else {
            ++i;
        }
    }
    if (numPending == kMaxPending)
        return;

    out.push_back({sampleOffset, true, src.channel, pitch, velocity});
    pending[static_cast<size_t>(numPending++)] = {stepPpq + length * gate * settings.rateBeats,
                                                   src.channel, pitch};
    prevNote = pitch;
}

void ArpEngine::fireStep(long stepIndex, double stepPpq, int sampleOffset, const CompiledSheet& sheet,
                          std::vector<MidiOut>& out)
{
    const int numSteps = std::clamp(settings.numSteps, 1, kMaxSteps);
    lastStep = static_cast<int>(stepIndex % numSteps); // playhead moves even with no notes held
    if (numHeld == 0)
        return;
    if (seqDirty || builtMode != settings.mode || builtOctaves != settings.octaves)
        buildSequence();
    if (seqLen == 0)
        return;

    const int patternStep = lastStep;
    if (!sheet.isStepActive(patternStep)) {
        ++sequencePos; // rests still advance the order, like a tracker row
        return;
    }

    if (settings.mode == Mode::Chord) {
        for (int i = 0; i < seqLen; ++i)
            playNote(seq[static_cast<size_t>(i)], patternStep, stepPpq, sampleOffset, sheet, out);
        return;
    }

    size_t idx = settings.mode == Mode::Random ? nextRandom() % static_cast<uint32_t>(seqLen)
                                               : static_cast<size_t>(sequencePos % static_cast<uint64_t>(seqLen));
    ++sequencePos;
    playNote(seq[idx], patternStep, stepPpq, sampleOffset, sheet, out);
}

void ArpEngine::flushAllOffs(int sampleOffset, std::vector<MidiOut>& out)
{
    for (int i = 0; i < numPending; ++i)
        out.push_back({sampleOffset, false, pending[static_cast<size_t>(i)].channel,
                       pending[static_cast<size_t>(i)].pitch, 0});
    numPending = 0;
}

void ArpEngine::emitDueOffs(double ppqStart, double beatsPerSample, int numSamples,
                             std::vector<MidiOut>& out)
{
    for (int i = 0; i < numPending;) {
        auto& p = pending[static_cast<size_t>(i)];
        const long offset = std::lround((p.ppq - ppqStart) / beatsPerSample);
        if (offset < numSamples) {
            out.push_back({static_cast<int>(std::max(0L, offset)), false, p.channel, p.pitch, 0});
            p = pending[static_cast<size_t>(--numPending)];
        } else {
            ++i;
        }
    }
}

void ArpEngine::process(const Transport& transport, const CompiledSheet& sheet, int numSamples,
                         std::vector<MidiOut>& out)
{
    if (numSamples <= 0)
        return;
    const size_t firstNew = out.size();

    if (!settings.enabled) {
        if (wasEnabled)
            flushAllOffs(0, out);
        wasEnabled = false;
        return;
    }
    wasEnabled = true;

    const double bpm = transport.bpm > 0.0 ? transport.bpm : 120.0;
    const double beatsPerSample = bpm / 60.0 / sampleRate;

    double ppqStart;
    if (transport.playing) {
        ppqStart = transport.ppqStart;
        // Host started, looped or located: drop hanging notes.
        if (!wasPlaying || std::abs(ppqStart - lastPpqEnd) > 1e-3)
            flushAllOffs(0, out);
    } else {
        if (wasPlaying)
            flushAllOffs(0, out);
        ppqStart = freePpq;
    }
    wasPlaying = transport.playing;

    const double ppqEnd = ppqStart + numSamples * beatsPerSample;
    const double rate = std::max(1.0 / 64.0, settings.rateBeats);
    const double swingOffset = std::clamp(settings.swing, 0.0, 1.0) * rate * 0.5;

    emitDueOffs(ppqStart, beatsPerSample, numSamples, out);

    // Step s sounds at s*rate, plus the swing offset on odd steps.
    for (long s = static_cast<long>(std::floor((ppqStart - swingOffset) / rate)) - 1;; ++s) {
        const double gridPpq = s * rate;
        if (gridPpq > ppqEnd)
            break;
        const double stepPpq = gridPpq + ((s & 1) ? swingOffset : 0.0);
        // Decide block membership in whole samples so timing is identical for any buffer size.
        const long offset = std::lround((stepPpq - ppqStart) / beatsPerSample);
        if (s < 0 || offset < 0 || offset >= numSamples)
            continue;
        fireStep(s, stepPpq, static_cast<int>(offset), sheet, out);
    }

    emitDueOffs(ppqStart, beatsPerSample, numSamples, out); // very short gates

    freePpq = (numHeld == 0 && !transport.playing) ? 0.0 : ppqEnd; // free clock restarts per chord
    lastPpqEnd = ppqEnd;

    // Stable insertion sort: std::stable_sort may allocate, and blocks hold few events.
    auto before = [](const MidiOut& a, const MidiOut& b) {
        if (a.sampleOffset != b.sampleOffset)
            return a.sampleOffset < b.sampleOffset;
        return !a.noteOn && b.noteOn; // offs before ons
    };
    for (size_t i = firstNew + 1; i < out.size(); ++i) {
        MidiOut e = out[i];
        size_t j = i;
        for (; j > firstNew && before(e, out[j - 1]); --j)
            out[j] = out[j - 1];
        out[j] = e;
    }
}

} // namespace arp
