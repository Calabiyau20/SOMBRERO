#!/usr/bin/env bash
# Builds Calabi Hats on macOS: VST3, AU (.component), CLAP and Standalone,
# universal (arm64 + x86_64). Needs Xcode command line tools and CMake.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel "$(sysctl -n hw.ncpu)"

echo
echo "Built into build/CalabiHats_artefacts/Release/"
echo "COPY_PLUGIN_AFTER_BUILD already installed them into:"
echo "  ~/Library/Audio/Plug-Ins/VST3/Calabi Hats.vst3"
echo "  ~/Library/Audio/Plug-Ins/Components/Calabi Hats.component"
echo "  ~/Library/Audio/Plug-Ins/CLAP/Calabi Hats.clap"
echo
echo "Ad-hoc signing so macOS lets the plugins load:"
for p in "$HOME/Library/Audio/Plug-Ins/VST3/Calabi Hats.vst3" \
         "$HOME/Library/Audio/Plug-Ins/Components/Calabi Hats.component" \
         "$HOME/Library/Audio/Plug-Ins/CLAP/Calabi Hats.clap"; do
  [ -e "$p" ] && codesign --force --deep -s - "$p" && echo "  signed: $p"
done
