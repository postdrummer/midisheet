#!/bin/bash
set -e

echo "=== Midisheet Build Script (macOS/Linux) ==="

# Create build directory
mkdir -p build
cd build

# Configure (JUCE is fetched automatically via FetchContent)
echo "Configuring with CMake..."
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
cmake --build . --config Release -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== Build Complete ==="
echo "Plugin locations:"
echo "  VST3: build/Midisheet_artefacts/Release/VST3/Midisheet.vst3"
echo "  AU:   build/Midisheet_artefacts/Release/AU/Midisheet.component"
echo "  Standalone: build/Midisheet_artefacts/Release/Standalone/Midisheet.app"
