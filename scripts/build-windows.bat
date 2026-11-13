@echo off
REM Builds Messy Hat Tricks on Windows: VST3, CLAP and Standalone.
REM Needs Visual Studio 2022 (Desktop C++ workload) and CMake.
cd /d "%~dp0.."
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
echo.
echo Built into build\MessyHatTricks_artefacts\Release\
echo Copy "Messy Hat Tricks.vst3" into C:\Program Files\Common Files\VST3\
echo Copy "Messy Hat Tricks.clap" into C:\Program Files\Common Files\CLAP\
pause
