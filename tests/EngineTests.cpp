#include "Check.h"
#include "engine/ArpEngine.h"
#include "sheet/Sheet.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace arp;

namespace {

// 120 bpm at 48 kHz: 24000 samples per beat, so a 1/16 step is 6000 samples.
constexpr double kSr = 48000.0;
constexpr double kBpm = 120.0;

struct Event {
    long sample; // absolute sample position
    bool on;
    int pitch;
    int velocity;
};

// Runs the engine with the transport playing from ppq 0 and returns events
// with absolute sample times.
std::vector<Event> run(ArpEngine& e, const CompiledSheet& sheet, long totalSamples, int block = 512,
                       bool playing = true)
{
    std::vector<Event> events;
    std::vector<MidiOut> out;
    out.reserve(1024);
    const double beatsPerSample = kBpm / 60.0 / kSr;
    for (long pos = 0; pos < totalSamples; pos += block) {
        int n = static_cast<int>(std::min<long>(block, totalSamples - pos));
        out.clear();
        e.process({pos * beatsPerSample, kBpm, playing}, sheet, n, out);
        for (auto& m : out) {
            CHECK(m.sampleOffset >= 0 && m.sampleOffset < n); // never outside the block
            events.push_back({pos + m.sampleOffset, m.noteOn, m.pitch, m.velocity});
        }
    }
    return events;
}

std::vector<Event> ons(const std::vector<Event>& events)
{
    std::vector<Event> r;
    for (auto& ev : events)
        if (ev.on)
            r.push_back(ev);
    return r;
}

// Builds a CompiledSheet from optional column default formulas.
CompiledSheet makeSheet(const char* velocity = "", const char* note = "", const char* gate = "",
                        const char* length = "")
{
    Sheet sheet;
    // Lookups include hidden columns so the helper can configure a parameter
    // (and show it) even when it starts hidden, e.g. Note.
    if (*velocity) {
        int c = sheet.findColumnByType(ColumnType::Velocity, false);
        if (c >= 0) sheet.setColumnDefaultFormula(c, velocity);
    }
    if (*note) {
        int c = sheet.findColumnByType(ColumnType::Note, false);
        if (c >= 0) {
            sheet.setColumnVisible(c, true);
            sheet.setColumnDefaultFormula(c, note);
        }
    }
    if (*gate) {
        int c = sheet.findColumnByType(ColumnType::Gate, false);
        if (c >= 0) sheet.setColumnDefaultFormula(c, gate);
    }
    if (*length) {
        int c = sheet.findColumnByType(ColumnType::Length, false);
        if (c >= 0) sheet.setColumnDefaultFormula(c, length);
    }
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());
    return std::move(*compiled);
}

ArpEngine held(std::initializer_list<int> pitches, int velocity = 100)
{
    ArpEngine e;
    e.prepare(kSr);
    for (int p : pitches)
        e.noteOn(1, p, velocity);
    return e;
}

void checkPitches(const std::vector<Event>& events, std::initializer_list<int> expect)
{
    auto on = ons(events);
    CHECK(on.size() >= expect.size());
    size_t i = 0;
    for (int p : expect) {
        if (i < on.size() && on[i].pitch != p)
            std::fprintf(stderr, "  step %zu: got %d, want %d\n", i, on[i].pitch, p);
        CHECK(i < on.size() && on[i].pitch == p);
        ++i;
    }
}

void testFollowsHeldNotes()
{
    // Regression: the old default pattern played a fixed C major scale
    // regardless of which notes were held.
    auto e = held({64, 60, 67});
    checkPitches(run(e, makeSheet(), 6000 * 6), {60, 64, 67, 60, 64, 67});
}

void testStepTimingAtSampleRate()
{
    // Regression: sample rate was never taken from prepareToPlay (stuck at 44.1k).
    auto e = held({60});
    auto on = ons(run(e, makeSheet(), 6000 * 4));
    CHECK(on.size() == 4);
    for (size_t i = 0; i < on.size(); ++i)
        CHECK(on[i].sample == static_cast<long>(i) * 6000);
}

void testBlockSizeIndependent()
{
    // Same absolute timing regardless of host buffer size.
    auto a = held({60, 63});
    auto b = held({60, 63});
    auto ea = run(a, makeSheet(), 48000, 64);
    auto eb = run(b, makeSheet(), 48000, 1024);
    CHECK(ea.size() == eb.size());
    for (size_t i = 0; i < std::min(ea.size(), eb.size()); ++i)
        CHECK(ea[i].sample == eb[i].sample && ea[i].pitch == eb[i].pitch && ea[i].on == eb[i].on);
}

void testNotesBalanced()
{
    // Regression: note-offs were scheduled past the end of the block.
    auto e = held({60, 64, 67});
    auto sheet = makeSheet("", "", "", "=IF(MOD(STEP,8)=7,2,1)");
    std::vector<Event> events = run(e, sheet, 48000);
    e.noteOff(1, 60); e.noteOff(1, 64); e.noteOff(1, 67);
    std::vector<MidiOut> out;
    out.reserve(256);
    e.process({2.0, kBpm, true}, sheet, 24000, out);
    int balance = 0;
    for (auto& ev : events) balance += ev.on ? 1 : -1;
    for (auto& m : out) balance += m.noteOn ? 1 : -1;
    CHECK(balance == 0);
}

void testGateAndLength()
{
    auto e = held({60});
    e.settings.gate = 0.5;
    auto events = run(e, makeSheet("", "", "", "=IF(STEP=1,2,1)"), 6000 * 2);
    // step 0: 0..3000 ; step 1: length 2 * gate 0.5 = one full step, 6000..12000
    CHECK(events.size() >= 3);
    CHECK(!events[1].on && events[1].sample == 3000);
    auto e2 = held({60});
    auto ev2 = run(e2, makeSheet("", "", "=25"), 6000);
    CHECK(ev2.size() == 2 && ev2[1].sample == 1500); // gate formula in percent
}

void testVelocityFormulaUsesInputVelocity()
{
    // Regression: VELOCITY was hard-coded to 100.
    auto e = held({60}, 70);
    auto on = ons(run(e, makeSheet("=IF(MOD(STEP,4)=0,127,VELOCITY)"), 6000 * 4));
    CHECK(on.size() == 4);
    CHECK(on[0].velocity == 127 && on[1].velocity == 70 && on[3].velocity == 70);
}

void testPrevAndNote()
{
    auto e = held({60, 62});
    // NOTE is the arp note; PREV the previously played pitch.
    checkPitches(run(e, makeSheet("", "=NOTE+12"), 6000 * 2), {72, 74});
}

void testModes()
{
    {
        auto e = held({60, 64, 67});
        e.settings.mode = Mode::UpDown;
        checkPitches(run(e, makeSheet(), 6000 * 6), {60, 64, 67, 64, 60, 64});
    }
    {
        auto e = held({60, 64, 67});
        e.settings.mode = Mode::DownUp;
        checkPitches(run(e, makeSheet(), 6000 * 5), {67, 64, 60, 64, 67});
    }
    {
        auto e = held({67, 60, 64});
        e.settings.mode = Mode::Order;
        checkPitches(run(e, makeSheet(), 6000 * 3), {67, 60, 64});
    }
    {
        auto e = held({60, 64});
        e.settings.mode = Mode::Down;
        e.settings.octaves = 2;
        checkPitches(run(e, makeSheet(), 6000 * 4), {76, 72, 64, 60});
    }
    {
        // Regression: Chord mode was a no-op.
        auto e = held({60, 64, 67});
        e.settings.mode = Mode::Chord;
        auto on = ons(run(e, makeSheet(), 6000));
        CHECK(on.size() == 3);
        for (auto& ev : on)
            CHECK(ev.sample == 0);
    }
}

void testSwing()
{
    // Regression: swing was ignored.
    auto e = held({60});
    e.settings.swing = 0.5; // odd steps pushed by a quarter step
    auto on = ons(run(e, makeSheet(), 6000 * 4));
    CHECK(on.size() == 4);
    CHECK(on[0].sample == 0 && on[1].sample == 7500 && on[2].sample == 12000 && on[3].sample == 19500);
}

void testInactiveStepsAndNumSteps()
{
    auto e = held({60});
    e.settings.numSteps = 2;
    Sheet sheet;
    sheet.setStepActive(1, false);
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    auto on = ons(run(e, *compiled, 6000 * 4));
    CHECK(on.size() == 2 && on[0].sample == 0 && on[1].sample == 12000);
}

void testTransportJumpFlushesNotes()
{
    auto e = held({60});
    e.settings.gate = 1.0;
    std::vector<MidiOut> out;
    out.reserve(64);
    e.process({0.0, kBpm, true}, makeSheet(), 512, out);
    CHECK(out.size() == 1 && out[0].noteOn);
    out.clear();
    e.process({8.0, kBpm, true}, makeSheet(), 512, out); // loop/locate
    CHECK(!out.empty() && !out[0].noteOn && out[0].sampleOffset == 0);
}

void testDisabledFlushesAndStaysQuiet()
{
    auto e = held({60});
    e.settings.gate = 1.0;
    std::vector<MidiOut> out;
    out.reserve(64);
    e.process({0.0, kBpm, true}, makeSheet(), 512, out);
    e.settings.enabled = false;
    out.clear();
    e.process({512 * kBpm / 60.0 / kSr, kBpm, true}, makeSheet(), 512, out);
    CHECK(out.size() == 1 && !out[0].noteOn);
    out.clear();
    e.process({1.0, kBpm, true}, makeSheet(), 24000, out);
    CHECK(out.empty());
}

void testFreeRunsWhenStopped()
{
    auto e = held({60});
    CHECK(ons(run(e, makeSheet(), 6000 * 3, 512, /*playing*/ false)).size() == 3);
}

} // namespace

void runEngineTests()
{
    testFollowsHeldNotes();
    testStepTimingAtSampleRate();
    testBlockSizeIndependent();
    testNotesBalanced();
    testGateAndLength();
    testVelocityFormulaUsesInputVelocity();
    testPrevAndNote();
    testModes();
    testSwing();
    testInactiveStepsAndNumSteps();
    testTransportJumpFlushesNotes();
    testDisabledFlushesAndStaysQuiet();
    testFreeRunsWhenStopped();
}
