@echo off
REM Builds Calabi Hats on Windows: VST3, CLAP and Standalone.
REM Needs Visual Studio 2022 (Desktop C++ workload) and CMake.
cd /d "%~dp0.."
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
echo.
echo Built into build\CalabiHats_artefacts\Release\
echo Copy "Calabi Hats.vst3" into C:\Program Files\Common Files\VST3\
echo Copy "Calabi Hats.clap" into C:\Program Files\Common Files\CLAP\
pause
