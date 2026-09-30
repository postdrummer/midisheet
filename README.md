# ArpExcel - VST Arpeggiator with Excel-like Functions

A VST3/AU MIDI arpeggiator plugin built with JUCE that supports Excel-like formulas for pattern generation.

## Features

- **Excel-like formula engine**: Use cell references (A1, B2), arithmetic (+, -, *, /, %, ^), and functions (MOD, IF, SUM, AVG, MIN, MAX, ABS, ROUND, FLOOR, CEIL, RANDOM)
- **7 arpeggiator modes**: Up, Down, Up-Down, Down-Up, Random, Order, Chord
- **7 rate options**: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64
- **Up to 64 steps** with individual note/velocity/gate/length formulas
- **Host-synced**: steps lock to the host's bar/beat grid, follow loops and locates, and free-run when the transport is stopped
- **Cross-platform**: VST3 + AU MIDI FX (macOS, loads in Logic's MIDI FX slot), VST3 + Standalone (Windows)

## Building

### macOS / Linux

```bash
# Initialize (clones JUCE)
./init.sh

# Build
./build.sh
```

### Tests

```bash
cmake -B build -G Ninja && cmake --build build --target engine_tests
ctest --test-dir build --output-on-failure
```

### Windows

```batch
REM Initialize (clones JUCE)
init.bat

REM Build
build.bat
```

## Formula Syntax

Each step has four optional formulas. An empty formula uses the default in brackets.

| Formula  | Result                                    | Default                  |
|----------|-------------------------------------------|--------------------------|
| note     | MIDI note to play                         | the arp note (`NOTE`)    |
| velocity | 1–127                                     | the held note's velocity |
| gate     | percent of the note length, 1–100         | the Gate knob            |
| length   | note length in steps (0.1–16); a note sounds for `length × gate` steps | 1 |

The leading `=` is optional. Names are case-insensitive. Unknown names, wrong
argument counts and syntax errors are reported instead of silently evaluating to 0.

### Cell References
- `A1` … `H64` — cells in the pattern's cell grid (8 columns × 64 rows), saved with the project

### Variables
- `STEP` — Position in the pattern (0 to Num Steps − 1)
- `NOTE` — The note the arp picked for this step (after mode and octave range)
- `VELOCITY` — Velocity the note was played with
- `LENGTH` — This step's length, from the length formula
- `CHANNEL` — MIDI channel the note was played on
- `PREV` — The previously played note
- `RANDOM` — Random value in [0, 1), new on every use

### Functions
- `MOD(a, b)` — Modulo
- `IF(condition, trueVal, falseVal)` — Conditional
- `SUM(a, b, ...)` — Sum of arguments
- `AVG(a, b, ...)` — Average of arguments
- `MIN(a, b, ...)` — Minimum
- `MAX(a, b, ...)` — Maximum
- `ABS(a)` — Absolute value
- `ROUND(a, decimals)` — Round to decimals
- `FLOOR(a)` — Round down
- `CEIL(a)` — Round up
- `RANDOM(min, max)` — Random number in range

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%` (modulo), `^` (power)
- Comparison: `=`, `<>`, `<`, `>`, `<=`, `>=`

### Examples

```
=A1+12                    ' Octave up from cell A1
=IF(MOD(STEP,4)=0,60,62)  ' Alternate between notes
=NOTE+7                   ' Perfect fifth above input
=VELOCITY-10              ' Softer velocity
=50+MOD(STEP,4)*10        ' Varying gate length
=IF(STEP>7,127,80)        ' Accent second half
```

## Default Pattern

The default pattern arpeggiates the held notes, with:
- Velocity: `=IF(MOD(STEP,4)=0,127,80)` (accent every 4th)
- Length: `=IF(MOD(STEP,8)=7,2,1)` (longer note every 8th)

Formulas are compiled once when the pattern changes and evaluated on the audio
thread without allocating, so they are safe to use on every step.

## Project Structure

```
ArpExcel/
├── CMakeLists.txt          # CMake build configuration
├── build.sh                # macOS/Linux build script
├── build.bat               # Windows build script
├── init.sh                 # Project initializer (clones JUCE)
├── src/
│   ├── PluginProcessor.h/cpp   # JUCE processor: params, MIDI I/O, state
│   ├── PluginEditor.h/cpp      # Plugin UI
│   ├── ArpPattern.h/cpp        # Editable pattern (formula text + cells), save/load
│   └── engine/                 # Plain C++, no JUCE, unit-tested
│       ├── ArpEngine.h/cpp     # Host-synced, real-time-safe arpeggiator
│       ├── Formula.h/cpp       # Formula compiler + allocation-free evaluator
│       └── PatternExchange.h   # Lock-free UI → audio pattern handoff
└── tests/                  # Engine and formula tests (ctest)
```

## License

MIT
