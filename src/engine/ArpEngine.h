#pragma once

// Host-synced arpeggiator core. No JUCE dependency so it can be unit-tested.
//
// Timing is derived from the host's quarter-note position (or an internal
// clock when the transport is stopped), so steps stay locked to the grid
// across block sizes, loops and locates. Every event carries a sample offset
// inside the current block.
//
// The engine consumes a CompiledSheet: for each step it evaluates the visible
// columns left-to-right, each column potentially transforming the MIDI signal
// (Note, Shift, Octave, Velocity, Gate, Length, ...).

#include "Formula.h"
#include "../sheet/CompiledSheet.h"
#include "../sheet/Column.h"

#include <array>
#include <cstdint>
#include <vector>

namespace arp {

constexpr int kMaxSteps = kMaxRows;

struct StepInput {
    int channel = 1;
    int pitch = 60;
    int velocity = 100;
};

struct StepResult {
    bool playable = true; // false when the note lands outside 0..127
    int pitch = 60;
    int velocity = 100;
    double gate = 0.5;   // fraction of the note length
    double length = 1.0; // in steps
    // The Octave column decides this row's octave spread: 0 = none.
    // One note-on is emitted per offset (multiple octaves).
    int numOctaveOffsets = 1;
    int octaveOffsets[8] = {0}; // semitone offsets per octave copy
};

// Applies a step's columns to the note the arp picked. Shared by the engine
// and the editor's preview so both always agree. Real-time safe.
StepResult evaluateStep(const CompiledSheet& sheet, int patternStep, const StepInput& in, int prevNote,
                        double defaultGate, uint32_t* rng, double defaultLength = 1.0);

enum class Mode { Up, Down, UpDown, DownUp, Random, Order, Chord };

struct Settings {
    bool enabled = true;
    Mode mode = Mode::Up;
    double rateBeats = 0.25; // step length in quarter notes (0.25 = 1/16)
    int octaves = 1;
    double gate = 0.5;    // default gate as a fraction of the step
    double swing = 0.0;   // 0..1: delays odd steps by up to half a step
    double length = 1.0;  // default note length in steps when no Length column overrides
};

struct Transport {
    double ppqStart = 0.0;
    double bpm = 120.0;
    bool playing = false;
};

struct MidiOut {
    int sampleOffset = 0;
    bool noteOn = true;
    int channel = 1;
    int pitch = 60;
    int velocity = 0;
    bool isCC = false; // else note on/off
    int ccNumber = 0;
    int ccValue = 0;
};

class ArpEngine {
public:
    ArpEngine();

    void prepare(double sampleRate);
    void reset();

    void noteOn(int channel, int pitch, int velocity);
    void noteOff(int channel, int pitch);
    void allNotesOff();

    // Appends this block's events to `out`, sorted by sampleOffset.
    // Real-time safe provided `out` has spare capacity.
    void process(const Transport&, const CompiledSheet&, int numSamples, std::vector<MidiOut>& out);

    Settings settings;

    int heldCount() const { return numHeld; }
    int lastPatternStep() const { return lastStep; } // -1 before the first step

private:
    struct HeldNote { int channel, pitch, velocity; uint64_t order; };
    struct SeqNote { int channel, pitch, velocity; };
    struct PendingOff { double ppq; int channel, pitch; };

    static constexpr int kMaxHeld = 32;
    static constexpr int kMaxSeq = kMaxHeld * 8 * 2;
    static constexpr int kMaxPending = 256;

    void buildSequence();
    void fireRow(int row, double stepPpq, int sampleOffset, const CompiledSheet&, std::vector<MidiOut>&);
    void playNote(const SeqNote&, int patternStep, double stepPpq, int sampleOffset, const CompiledSheet&, std::vector<MidiOut>&);
    void emitDueOffs(double ppqStart, double beatsPerSample, int numSamples, std::vector<MidiOut>&);
    void flushAllOffs(int sampleOffset, std::vector<MidiOut>&);
    uint32_t nextRandom();

    // Step-slot position of a row in units of rateBeats. Custom Time value
    // when the Time column is visible and the cell is explicitly set;
    // otherwise the row's default position.
    double rowSlot(int row, const CompiledSheet& sheet, formula::Context& ctx) const;

    double sampleRate = 44100.0;

    std::array<HeldNote, kMaxHeld> held{};
    int numHeld = 0;
    std::array<SeqNote, kMaxSeq> seq{};
    int seqLen = 0;
    bool seqDirty = true;
    Mode builtMode = Mode::Up;
    int builtOctaves = 1;

    std::array<PendingOff, kMaxPending> pending{};
    int numPending = 0;

    uint64_t arrivalCounter = 0;
    uint64_t sequencePos = 0;
    int prevNote = 0;
    int lastStep = -1;
    uint32_t rngState = 0x9e3779b9u;

    double freePpq = 0.0;
    double lastPpqEnd = -1.0;
    bool wasPlaying = false;
    bool wasEnabled = true;
};

} // namespace arp
