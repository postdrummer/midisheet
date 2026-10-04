#pragma once

// Column system for the spreadsheet-arpeggiator.
//
// A Column is a parameter (Note, Shift, Octave, Velocity, Gate, Length, Time,
// Chance, CC, ...). Columns can be added, removed, shown, hidden, renamed,
// reordered, and assigned a data type. Each column has a default value (and
// optional default formula) used when the column is hidden or a cell is empty.
//
// ColumnMeta is the compact, audio-thread-safe form of a Column: no strings,
// no allocation, fixed size.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace arp {

// Fixed limits for the compiled (audio-thread) representation. The editable
// sheet can grow columns up to kMaxColumns; rows are fixed at kMaxRows.
constexpr int kMaxColumns = 64;
constexpr int kMaxRows = 64;

enum class ColumnType : uint8_t {
    Number,   // generic numeric value
    Note,     // MIDI note number (0..127) or note name; <0 = pass through
    Shift,    // semitone shift
    Octave,   // octave shift (value * 12 semitones)
    Velocity, // 0..127; <=0 = use incoming note velocity
    Gate,     // 0..100 percent of the step
    Length,   // note length in steps
    Percent,  // 0..100
    Chance,   // 0..100 probability
    Time,     // beat offset / subdivision position
    CC,       // MIDI CC: cell value is the CC value, ccNumber selects the CC
    Text,     // label / annotation (not evaluated)
    Formula,  // formula-only column (a modulation source)
};

inline const char* columnTypeName(ColumnType t)
{
    switch (t) {
        case ColumnType::Number: return "Number";
        case ColumnType::Note: return "Pitch";
        case ColumnType::Shift: return "Shift";
        case ColumnType::Octave: return "Octave";
        case ColumnType::Velocity: return "Velocity";
        case ColumnType::Gate: return "Gate";
        case ColumnType::Length: return "Length";
        case ColumnType::Percent: return "Percent";
        case ColumnType::Chance: return "Chance";
        case ColumnType::Time: return "Time";
        case ColumnType::CC: return "CC";
        case ColumnType::Text: return "Text";
        case ColumnType::Formula: return "Formula";
    }
    return "?";
}

// A column in the editable sheet (message thread). Owns its name and default
// formula strings; the compiled form (ColumnMeta) strips those.
struct Column {
    std::string name;                  // user-editable, shown under the type
    ColumnType type = ColumnType::Number;
    bool visible = true;
    double defaultValue = 0.0;         // used when hidden or a cell is empty
    std::string defaultFormula;        // evaluated when a cell is empty
    int ccNumber = 0;                  // for ColumnType::CC
};

// Compact column metadata shared with the audio thread. Fixed size, no strings.
struct ColumnMeta {
    ColumnType type = ColumnType::Number;
    bool visible = true;
    double defaultValue = 0.0;
    int ccNumber = 0;
};

// Convert a 0-based column index to spreadsheet letters: 0 -> A, 25 -> Z,
// 26 -> AA, 27 -> AB, ...
inline std::string columnLetters(int col)
{
    std::string s;
    for (int c = col + 1; c > 0; c = (c - 1) / 26)
        s = static_cast<char>('A' + (c - 1) % 26) + s;
    return s;
}

// Convert spreadsheet letters to a 0-based column index (-1 if invalid).
inline int columnIndex(const std::string& letters)
{
    auto s = letters;
    // Trim and uppercase.
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    s.erase(s.find_last_not_of(" \t\n\r") + 1);
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (s.empty())
        return -1;
    int col = 0;
    for (char c : s) {
        if (c < 'A' || c > 'Z')
            return -1;
        col = col * 26 + (c - 'A' + 1);
    }
    return col - 1;
}

} // namespace arp
