# Midisheet - VST/AU MIDI Arpeggiator with a Spreadsheet Pattern Editor

A VST3/AU MIDI arpeggiator built with JUCE. The pattern is a spreadsheet: rows
are steps, columns are typed parameters, and every cell can hold an Excel-like
formula that the audio thread evaluates in real time.

## Features

- **Spreadsheet pattern** - rows are steps, columns are typed parameters (Pitch,
  Shift, Octave, Velocity, Gate, Time, Repeat, Chance, Note, ...). Columns can
  be added, deleted, renamed, re-typed, hidden and drag-reordered; each one has
  a default value/formula that empty cells fall back to.
- **Excel-like formula engine** - cell refs (`A1`), name refs (`Shift[STEP]`),
  arithmetic, comparisons and functions (`IF`, `MOD`, `SUM`, `AVG`, `MIN`,
  `MAX`, `ABS`, `ROUND`, `FLOOR`, `CEIL`, `RANDOM`, `NOTE`). Formulas compile
  once on the message thread and evaluate on the audio thread without
  allocating.
- **7 arpeggiator modes** - Up, Down, Up-Down, Down-Up, Random, Order, Chord.
- **14 rates** - 1/1 ... 1/64 plus triplet divisions (1/1T ... 1/64T).
- **Host-synced timing** - steps lock to the host's bar/beat grid across block
  sizes, loops and locates, and free-run when the transport is stopped.
- **Sheet editing** - undo/redo, TSV copy/paste, row and column context menus,
  value scrubbing, autocomplete, save/load (`.midisheet` / `.json`), Restore
  Defaults, and a hover help bar.
- **Everything is remembered** - the parameters and the whole sheet are stored
  in the host project; the sheet can also be saved to a standalone file.
- **Formats** - macOS: VST3, AU MIDI FX (loads in Logic's MIDI FX slot) and
  Standalone; Windows: VST3 and Standalone. The VST3 carries a silent stereo
  output so hosts like Ableton accept it as an instrument.
- **CI** - macOS (arm64 + x86_64) and Windows builds run `ctest`, validate the
  AU with `auval`, and publish tagged GitHub Releases.

## Building

JUCE 8.0.4 is fetched automatically by CMake (`FetchContent`), so a plain
configure + build is enough:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

`./build.sh` runs the same steps but forces the Ninja generator
(`brew install ninja` first); `build.bat` configures with Visual Studio 2022
on Windows. `init.sh` (which clones JUCE into the repo) is optional legacy
helper - CMake does not use it.

Artefacts:

| Format | Path |
|---|---|
| VST3 | `build/Midisheet_artefacts/Release/VST3/Midisheet.vst3` |
| AU | `build/Midisheet_artefacts/Release/AU/Midisheet.component` |
| Standalone | `build/Midisheet_artefacts/Release/Standalone/Midisheet.app` |

`COPY_PLUGIN_AFTER_BUILD` also installs the VST3 and AU into
`~/Library/Audio/Plug-Ins/{VST3,Components}` on every build.

### Tests

```bash
cmake --build build --target engine_tests
ctest --test-dir build --output-on-failure
```

### AU validation

```bash
auval -v aumi Mdsh Mids
```

### CI and releases

`.github/workflows/build.yml` builds macOS (universal) and Windows on every
push and pull request: build -> `ctest` -> `auval` -> artefacts. Pushes to
`master`/`main` publish Releases: `macos-v<N>` (VST3 + AU) and `windows-v<N>`
(VST3).

## The window

Three bands, top to bottom:

1. **Top panel** - three groups, each control explaining itself in the status
   bar as you hover it:
   - *File*: Undo, Redo, Restore Defaults (asks before replacing the sheet),
     Load, Save, Save As...
   - *Defaults*: the default used by empty cells, per column type - Note
     (On/Off), Pitch (Midi In / note names), Velocity (Midi In / 0-128),
     Gate %, Chance %, Shift, Octave, Time (style + division), Repeat (1-16).
   - *Edit*: add N rows/columns, delete the active column, merge the selected
     cells vertically, change the active column's data type.
2. **Name box + formula bar** - the name box shows the selection (`A1`, or
   `A1:D6` for a range); type a reference and press Enter to jump there. The
   formula bar shows the selected cell's formula and pops up completions for
   functions, variables, column names and cell references.
3. **The grid** - one row per step, one column per sheet column. Headers show
   the data type over the column name; the row strip on the left shows the step
   numbers. Each cell shows the value the step produces for a preview chord
   (C E G, velocity 100, Up order), and the playing row is highlighted. Dim
   cells use the column default or sit outside the pattern, `ERR` marks a
   formula that doesn't compile (the message is in the status bar), and a
   trailing `~` marks a formula that uses `RANDOM`. `+ Add row` under the last
   row and the `+` strip on the right add rows and columns.

The window height auto-fits the number of rows.

### Keys

| Keys | Action |
|---|---|
| arrows, Tab / Shift-Tab | move / extend the selection |
| PageUp / PageDown | page up / down |
| Home / End | first / last step |
| Ctrl-d / Ctrl-u | half page down / up |
| Esc | collapse the selection to a single cell |
| Enter, F2, double-click | edit the formula (Enter commits, Esc cancels) |
| any printable character | start typing a formula (spreadsheet style) |
| Delete / Backspace | clear the selected cells |
| Space | toggle the step on/off |
| Cmd/Ctrl+C / Cmd/Ctrl+V | copy / paste cells (TSV) |
| Cmd/Ctrl+Z | undo |
| Cmd/Ctrl+Shift+Z, Ctrl+R | redo |
| Cmd/Ctrl+Shift+= / - | add or delete rows/columns (follows the last header or row strip you selected) |
| Cmd/Ctrl+S | save |

### Mouse

- Click a cell, a column header, a row number or the corner square to select;
  Shift-click extends the selection.
- Drag a column header to reorder the column.
- Drag vertically on a cell to scrub its value (one unit per 12 px, clamped to
  the column's type).
- Right-click a column header: data type, Rename..., Hide/Show, Add column to
  the left/right, Delete column.
- Right-click a row number: insert row above/below, delete, move up/down,
  hide/show, copy row, paste row.
- Scroll with the wheel. The top-panel dropdowns also accept a vertical drag
  while held to cycle values.

## Column types

Columns are evaluated left to right, so order matters. Hiding a column is
cosmetic: its values still apply.

| Type | Effect on a step |
|---|---|
| Note | Boolean step gate: `<= 0.5` rests, empty (the default) plays |
| Pitch | `>= 0` sets the step's note (MIDI number or note name); negative = use the arp's note |
| Shift | Semitone offset added to the pitch |
| Octave | Number of octave copies (1-8); a negative value `-n` plays `n + 1` octaves |
| Velocity | 1-127 sets the velocity; `<= 0` passes the incoming velocity through |
| Gate | Gate as a percent of the step (0-100) |
| Chance | Probability (0-100 %) that the step sounds; a miss counts as a rest |
| Time | Moves the row to a custom step slot; rows with no explicit Time value keep their own step |
| Repeat | Repeat count for the step (1-16) - *planned: not applied by the engine yet* |
| CC | MIDI CC out: the cell is the value, the column's CC number is the controller - *will be removed* |
| Number | Generic number for formulas to read |
| Text | Label / annotation, not evaluated |
| Formula | Formula-only column (a modulation source) |

The default sheet ships with Pitch, Shift, Octave, Velocity and Gate visible,
Time, Repeat and Chance hidden, 16 steps, and every step active.

## Formula syntax

The leading `=` is optional, names are case-insensitive, and unknown names,
wrong argument counts and syntax errors are reported instead of silently
evaluating to 0.

### Cell references

- `A1` ... `BL64` - cells in the sheet (64 columns x 64 rows); `$A$1` is
  accepted and behaves exactly like `A1`.
- `Shift[STEP]`, `Note[0]` - a column by name plus a 0-based row expression,
  matched case-insensitively against the sheet's column names.
- A cell may only read columns to its own left. Hidden columns, forward
  references and circular references (A1 = B1, B1 = A1) fall back to the
  column default, and the chain is depth-capped, so evaluation always
  terminates.

### Variables

- `STEP` / `ROW` - position in the pattern (0 to rows - 1)
- `NOTE` - the note the arp picked for this step (after mode and octave range)
- `VELOCITY` - velocity the note was played with
- `LENGTH` - the Length parameter, note length in steps
- `CHANNEL` - MIDI channel of the input note
- `PREV` - the previously played note
- `RANDOM` / `RAND` - random value in [0, 1), new on every use
- `TRUE` / `FALSE` - 1 and 0

### Functions

- `MOD(a, b)` - modulo
- `IF(condition, trueVal, falseVal)` - conditional
- `SUM(a, b, ...)` / `AVG(...)` / `AVERAGE(...)` - sum / average
- `MIN(...)` / `MAX(...)` - smallest / largest argument
- `ABS(a)` - absolute value
- `ROUND(a, decimals)` - round to `decimals` places
- `FLOOR(a)` / `CEIL(a)` - round down / up
- `RANDOM(min, max)` / `RAND(min, max)` - random number in range
- `NOTE()` / `PREV()` - zero-arg aliases of the variables
- `NOTE("C3")` - parse a note name into a MIDI note number

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%` (modulo), `^` (power)
- Comparison: `=`, `<>`, `<`, `>`, `<=`, `>=`

A Pitch cell may also be written as a note name (`C3`, `=F#4`) instead of a
formula.

### Examples

| Formula | Result |
|---|---|
| `=NOTE+12` | octave up from the arp's picked note |
| `=IF(MOD(STEP,4)=0,60,62)` | alternate between two notes |
| `=VELOCITY-10` | softer than the incoming velocity |
| `=50+MOD(STEP,4)*10` | varying gate length |
| `=IF(STEP>7,127,80)` | accent the second half of the pattern |
| `=RANDOM(60,72)` | a different note on every evaluation |
| `=A1*2` | read a cell to the left |
| `=Shift[STEP]+12` | same, addressed by column name |

Formulas are compiled when the sheet changes and evaluated on the audio thread
without allocating, so they are safe on every step.

## Host parameters

The parameters below are exposed to the host for automation (they show up in
the host's generic parameter view; the in-window controls for them were
removed for now):

| Parameter | Values |
|---|---|
| Enabled | on/off |
| Mode | Up, Down, Up-Down, Down-Up, Random, Order, Chord |
| Rate | 1/1 ... 1/64 and 1/1T ... 1/64T |
| Gate | 0...1 (default for cells with no Gate column value) |
| Octave Range | 1...8 |
| Swing | 0...1 |
| Length | 0.1...16 steps |

## MIDI I/O

The processor filters incoming notes (channel, note range, velocity curve) and
writes to a chosen output channel, with replace-vs-augment routing and a MIDI
thru option. These currently live as settings on the processor with sane
defaults (all channels in, channel 1 out, linear curve, replace, thru off) -
there is no UI for them yet.

## Project structure

```
midisheet/
├── CMakeLists.txt              # CMake build configuration (fetches JUCE)
├── build.sh / build.bat        # build scripts (macOS/Linux, Windows)
├── init.sh                     # optional: clones JUCE next to the sources
├── .github/workflows/build.yml # CI: build, ctest, auval, releases
├── src/
│   ├── PluginProcessor.h/cpp   # APVTS parameters, MIDI I/O, state, sheet handoff
│   ├── PluginEditor.h/cpp      # window layout, formula bar, autocomplete, status
│   ├── TopPanel.h/cpp          # File | Defaults | Edit groups + hover help
│   ├── TrackerGrid.h/cpp       # spreadsheet grid: selection, editing, undo, menus
│   ├── SharpLNF.h              # sharp-cornered mono look and feel
│   ├── sheet/                  # the editable sheet (message thread)
│   │   ├── Column.h            # column types + A1 letters
│   │   ├── Sheet.h/cpp         # rows/cols/cells, defaults, compile, JSON save/load
│   │   └── CompiledSheet.h     # immutable snapshot for the audio thread
│   └── engine/                 # plain C++, no JUCE UI, unit-tested
│       ├── ArpEngine.h/cpp     # host-synced arpeggiator core
│       ├── Formula.h/cpp       # formula compiler + allocation-free evaluator
│       └── PatternExchange.h   # lock-free UI -> audio pattern handoff
└── tests/                      # ctest suites: engine, formula, sheet
```

## License

MIT
