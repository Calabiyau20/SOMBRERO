# Messy Hat Tricks

A hi-hat engine by **Calabi Yau**. Nothing is sampled: every hit is built from
three voiced layers, a short FM transient, a tuned comb, ring modulation and a
high-pass filter, then shaped by its own envelope.

- Nine titled voice modules, laid out in the order the signal travels
- Sixteen-step sequencer with pitch, velocity and **probability per step**
- Euclidean generator, drawn as the cycle it is
- Two modulation lanes routed by dragging them onto any knob
- MIDI output of the notes it generates
- Band-limited oscillators plus switchable oversampling (1× / 2× / 4×)

Formats: **VST3, Audio Unit, CLAP and standalone**, on macOS (universal,
Apple Silicon and Intel) and Windows 64-bit.

Installing: see [INSTALL.md](INSTALL.md).
Licence and credits: see [CREDITS.md](CREDITS.md) and [LICENSE](LICENSE).

---

## Building it yourself

CMake 3.22 or newer, and internet access the first time so it can fetch JUCE
and clap-juce-extensions.

```bash
./scripts/build-mac.sh         # macOS: Xcode command line tools
scripts\build-windows.bat      # Windows: Visual Studio 2022, Desktop C++
```

Both of these must be set **before** `project()` in `CMakeLists.txt`, because
CMake resolves the macOS toolchain while running it and silently ignores a value
set afterwards:

```cmake
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13")
```

Getting that wrong stamps the bundles with the build machine's SDK version, and
macOS then refuses to load them — the symptom is `OpenAComponent: result: -1`
from `auval`, with `built for macOS 26.0 which is newer than running OS` in
Console.

## Layout

```
CMakeLists.txt           JUCE + clap-juce-extensions, the four formats
Source/Engine.h          the voice DSP, the 16-step pattern, Euclidean maths
Source/PluginProcessor.* parameters, sequencer, modulation, oversampling, presets
Source/PluginEditor.*    the window and its layout
Source/Look.h            palette, materials and the hardware look and feel
Source/Widgets.h         knobs, modules, the XY pad, the ring, the step grid
Resources/               the artwork embedded in the binary
```

## How it works

**Signal flow.** Metal, click and hiss plus the FM transient are summed, then
pass through the comb, the high-pass filter, the ring modulator and the stereo
spread, and the amplitude envelope shapes the result.

**Modulation** resolves at the instant of each hit rather than continuously, so
two consecutive steps can sound completely different with no smearing between
them.

**Antialiasing** works on two levels: the six metal oscillators are band-limited
with PolyBLEP, and the whole voice render can run oversampled, because ring
modulation and the comb create sum and difference frequencies above Nyquist that
no BLEP can fix.

**The comb feedback** reaches 97 %. At that setting the loop barely loses energy,
so the return path is softly saturated: the coefficient is exact at low level and
compresses instead of running away at high level.
