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
            if (m.isCC)
                continue; // CC events are reported by other tests; skip note decoding
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
        int c = sheet.findColumnByType(ColumnType::Pitch, false);
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
    Sheet sheet;
    sheet.setNumRows(2);
    sheet.setStepActive(1, false);
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    auto on = ons(run(e, *compiled, 6000 * 4));
    CHECK(on.size() == 2 && on[0].sample == 0 && on[1].sample == 12000);
}

void testCustomTimeColumn()
{
    // Time column value replaces the row's default slot position.
    auto e = held({60});
    Sheet sheet;
    sheet.setNumRows(2);
    const int tc = sheet.findColumnByType(ColumnType::Time, false);
    sheet.setColumnVisible(tc, true);
    sheet.setCell(tc, 0, 0.5); // row 0 -> slot 0.5 instead of 0
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    auto on = ons(run(e, *compiled, 12000 * 2));
    // 16th = 6000 samples, two-step loop = 12000 samples.
    CHECK(on.size() >= 4);
    CHECK(on[0].sample == 3000); // row 0, slot 0.5
    CHECK(on[1].sample == 6000); // row 1, default slot 1
    CHECK(on[2].sample == 15000);
    CHECK(on[3].sample == 18000);
}

void testTimeRearrangesRows()
{
    // Two rows swapped: row 0 -> slot 1, row 1 -> slot 0.
    auto e = held({60});
    Sheet sheet;
    sheet.setNumRows(2);
    const int tc = sheet.findColumnByType(ColumnType::Time, false);
    sheet.setColumnVisible(tc, true);
    sheet.setCell(tc, 0, 1.0);
    sheet.setCell(tc, 1, 0.0);
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    auto on = ons(run(e, *compiled, 12000 * 2));
    CHECK(on.size() >= 4);
    CHECK(on[0].sample == 0);    // row 1 at slot 0
    CHECK(on[1].sample == 6000); // row 0 at slot 1
}

void testOctaveColumnExpandsPattern()
{
    // Octave = 2 emits the note at +0 and +12 within the same step.
    auto e = held({60});
    Sheet sheet;
    const int oc = sheet.findColumnByType(ColumnType::Octave, false);
    sheet.setColumnVisible(oc, true);
    sheet.setCell(oc, 0, 2.0);
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());

    auto ev = ons(run(e, *compiled, 6000 * 2));
    CHECK(ev.size() >= 2);
    CHECK(ev[0].pitch == 60 && ev[1].pitch == 72);

    // A literal "0-3" parses as -3 -> treated as range 0..3 = 4 octaves.
    auto e2 = held({60});
    Sheet sheet2;
    sheet2.setColumnVisible(oc, true);
    sheet2.setCell(oc, 0, -3.0);
    auto c2 = sheet2.compile(errors);
    auto ev2 = ons(run(e2, *c2, 6000));
    CHECK(ev2.size() == 4);
    for (int i = 0; i < 4; ++i)
        CHECK(ev2[i].pitch == 60 + i * 12);
}

void testChanceColumnSuppressesNotes()
{
    auto e = held({60});
    Sheet sheet;
    const int cc = sheet.findColumnByType(ColumnType::Chance, false);
    sheet.setColumnVisible(cc, true);
    sheet.setColumnDefault(cc, 0.0); // never
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());
    auto ev = ons(run(e, *compiled, 6000 * 2));
    CHECK(ev.empty());

    auto e2 = held({60});
    Sheet sheet2;
    const int cc2 = sheet2.findColumnByType(ColumnType::Chance, false);
    sheet2.setColumnVisible(cc2, true);
    sheet2.setCell(cc2, 0, 100.0); // always
    auto c2 = sheet2.compile(errors);
    auto ev2 = ons(run(e2, *c2, 6000 * 2));
    CHECK(ev2.size() == 2);
}

void testTempoChangeChangesStepDuration()
{
    // Fire with tempo=120, then immediately with tempo=60: from that point on,
    // step intervals should be half as long.
    auto e = held({60});
    auto sheet = makeSheet();
    std::vector<MidiOut> out;
    out.reserve(256);

    // Prime with 120 bpm for one full 16th step.
    e.process({0.0, 120.0, true}, sheet, 6000, out);
    out.clear();

    // Next step interval at 60 bpm is half a second (= 1/2 beat?? no: 120->60 halves frequency,
    // each 16th is now 12000 samples).
    e.process({0.125, 60.0, true}, sheet, 12000, out);
    int onAt = -1;
    for (auto& m : out)
        if (m.noteOn)
            onAt = m.sampleOffset;
    // Step 1 at 60bpm fires at 0.25 beats after 0.125 -> offset = (0.25-0.125)/(60/60/48000) = 6000
    CHECK(onAt == 6000);
}

void testTimeColumnCanDescribeSwing()
{
    // A Time formula that yields 0.5 step-offset on odd rows gives the same
    // pattern as engine swing=0.5.
    auto e = held({60});
    Sheet sheet;
    const int tc = sheet.findColumnByType(ColumnType::Time, false);
    sheet.setColumnVisible(tc, true);
    sheet.setColumnDefaultFormula(tc, "=IF(MOD(STEP,2)=1,STEP+0.25,STEP)"); // odd: + half the engine's swing
    juce::StringArray errors;
    auto compiled = sheet.compile(errors);
    CHECK(errors.isEmpty());
    auto on = ons(run(e, *compiled, 6000 * 4));
    CHECK(on.size() == 4);
    CHECK(on[0].sample == 0 && on[1].sample == 7500 && on[2].sample == 12000 && on[3].sample == 19500);
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
    testCustomTimeColumn();
    testTimeRearrangesRows();
    testOctaveColumnExpandsPattern();
    testChanceColumnSuppressesNotes();
    testTempoChangeChangesStepDuration();
    testTimeColumnCanDescribeSwing();
    testTransportJumpFlushesNotes();
    testDisabledFlushesAndStaysQuiet();
    testFreeRunsWhenStopped();
}
