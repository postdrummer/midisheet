#!/bin/bash
set -e

echo "=== Midisheet Project Initializer ==="

# Clone JUCE if not present
if [ ! -d "JUCE" ]; then
    echo "Cloning JUCE..."
    git clone --depth 1 https://github.com/juce-framework/JUCE.git
fi

# Initialize git repo if not already
if [ ! -d ".git" ]; then
    echo "Initializing git repo..."
    git init
    git add .
    git commit -m "Initial commit: Midisheet VST plugin"
fi

echo ""
echo "=== Project initialized ==="
echo "Run ./build.sh to build the plugin"
