@echo off
echo === ArpExcel Build Script (Windows) ===

REM Initialize JUCE submodule if not already done
if not exist "JUCE\.git" (
    echo Initializing JUCE submodule...
    git submodule update --init --recursive
)

REM Create build directory
if not exist build mkdir build
cd build

REM Configure
echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64

REM Build
echo Building...
cmake --build . --config Release

echo.
echo === Build Complete ===
echo Plugin locations:
echo   VST3: build\ArpExcel_artefacts\Release\VST3\ArpExcel.vst3
echo   Standalone: build\ArpExcel_artefacts\Release\Standalone\ArpExcel.exe
