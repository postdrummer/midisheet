#pragma once

// Host-synced arpeggiator core. No JUCE dependency so it can be unit-tested.
//
// Timing is derived from the host's quarter-note position (or an internal
// clock when the transport is stopped), so steps stay locked to the grid
// across block sizes, loops and locates. Every event carries a sample offset
// inside the current block.

#include "Formula.h"

#include <array>
#include <cstdint>
#include <vector>

namespace arp {

constexpr int kMaxSteps = 64;

struct Step {
    bool active = true;
    // Empty programs fall back to the arp note / held velocity / global gate / 1 step.
    formula::Program note, velocity, gate, length;
};

// Immutable once handed to the engine; see PatternExchange.
struct Pattern {
    std::array<Step, kMaxSteps> steps;
    std::array<double, formula::kCellCols * formula::kCellRows> cells{};
};

struct StepInput {
    int channel = 1;
    int pitch = 60;
    int velocity = 100;
};

struct StepResult {
    bool playable = true; // false when the note formula lands outside 0..127
    int pitch = 60;
    int velocity = 100;
    double gate = 0.5;   // fraction of the note length
    double length = 1.0; // in steps
};

// Applies a step's formulas to the note the arp picked. Shared by the engine
// and the editor's preview so both always agree. Real-time safe.
StepResult evaluateStep(const Pattern&, int patternStep, const StepInput&, int prevNote,
                        double defaultGate, uint32_t* rng);

enum class Mode { Up, Down, UpDown, DownUp, Random, Order, Chord };

struct Settings {
    bool enabled = true;
    Mode mode = Mode::Up;
    double rateBeats = 0.25; // step length in quarter notes (0.25 = 1/16)
    int octaves = 1;
    int numSteps = 16;
    double gate = 0.5;  // default gate as a fraction of the step
    double swing = 0.0; // 0..1: delays odd steps by up to half a step
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
    void process(const Transport&, const Pattern&, int numSamples, std::vector<MidiOut>& out);

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
    void fireStep(long stepIndex, double stepPpq, int sampleOffset, const Pattern&, std::vector<MidiOut>&);
    void playNote(const SeqNote&, int patternStep, double stepPpq, int sampleOffset, const Pattern&, std::vector<MidiOut>&);
    void emitDueOffs(double ppqStart, double beatsPerSample, int numSamples, std::vector<MidiOut>&);
    void flushAllOffs(int sampleOffset, std::vector<MidiOut>&);
    uint32_t nextRandom();

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
