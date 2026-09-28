# ArpExcel - VST Arpeggiator with Excel-like Functions

A VST3/AU MIDI arpeggiator plugin built with JUCE that supports Excel-like formulas for pattern generation.

## Features

- **Excel-like formula engine**: Use cell references (A1, B2), arithmetic (+, -, *, /, %, ^), and functions (MOD, IF, SUM, AVG, MIN, MAX, ABS, ROUND, FLOOR, CEIL, RANDOM)
- **7 arpeggiator modes**: Up, Down, Up-Down, Down-Up, Random, Order, Chord
- **7 rate options**: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64
- **Up to 64 steps** with individual note/velocity/gate/length formulas
- **Cross-platform**: VST3 + AU (macOS), VST3 + Standalone (Windows)

## Building

### macOS / Linux

```bash
# Initialize (clones JUCE)
./init.sh

# Build
./build.sh
```

### Windows

```batch
REM Initialize (clones JUCE)
init.bat

REM Build
build.bat
```

## Formula Syntax

### Cell References
- `A1`, `B2`, `AA10` — Reference cells in the pattern grid
- Set cell values via the formula engine or use them as note offsets

### Variables
- `STEP` — Current step index (0-based)
- `NOTE` — Current input note
- `VELOCITY` — Current input velocity
- `LENGTH` — Current step length
- `CHANNEL` — MIDI channel
- `PREV` — Previous note value
- `RANDOM` — Random value (0-1)

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

The default pattern uses a C major scale stored in cells A1-A16, with formulas that demonstrate:
- Note formulas: `=A1+12` (octave up)
- Velocity formulas: `=IF(MOD(STEP,4)=0,127,80)` (accent every 4th)
- Gate formulas: `=50+MOD(STEP,4)*10` (varying gate)
- Length formulas: `=IF(MOD(STEP,8)=7,2,1)` (longer note every 8th)

## Project Structure

```
ArpExcel/
├── CMakeLists.txt          # CMake build configuration
├── build.sh                # macOS/Linux build script
├── build.bat               # Windows build script
├── init.sh                 # Project initializer (clones JUCE)
├── src/
│   ├── PluginProcessor.h/cpp   # Main audio processor
│   ├── PluginEditor.h/cpp      # Plugin UI
│   ├── Arpeggiator.h/cpp       # Core arpeggiator logic
│   ├── ArpPattern.h/cpp        # Pattern data structure
│   └── FormulaEngine.h/cpp     # Excel-like formula parser/evaluator
└── JUCE/                   # JUCE framework (submodule)
```

## License

MIT
