# Calabi Hats

Motor de hi-hats de Calabi Yau: capas Metal / Click / Hiss, comb filter, transitorio FM,
ring modulation, filtro HP con Q y spread estéreo Haas — con arpegiador de 16 pasos
(On / Pitch / Vel / Prob), generador euclidiano y dos lanes de modulación ruteables
arrastrando chips sobre cualquier knob.

Formatos: **VST3**, **AU (.component)**, **CLAP** y **Standalone**, para **macOS**
(universal arm64 + x86_64) y **Windows** (x64; en Windows no existe AU).

---

## Compilar

Requisitos comunes: CMake 3.22+ y conexión a internet la primera vez (CMake descarga
JUCE 8.0.4 y clap-juce-extensions solo).

### macOS

```bash
./scripts/build-mac.sh
```

Necesita Xcode Command Line Tools (`xcode-select --install`). El script compila,
instala los plugins en `~/Library/Audio/Plug-Ins/` y los firma ad-hoc para que
Ableton y Logic los carguen sin pelear con Gatekeeper.

### Windows

```bat
scripts\build-windows.bat
```

Necesita Visual Studio 2022 con el workload "Desktop development with C++".

### Los dos a la vez, sin tener las dos máquinas

Subí el repo a GitHub y el workflow `.github/workflows/build.yml` compila en
macOS y Windows en cada push, y deja los binarios como artifacts descargables.

---

## Dónde queda cada formato

| Formato | macOS | Windows |
|---|---|---|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/Calabi Hats.vst3` | `C:\Program Files\Common Files\VST3\` |
| AU | `~/Library/Audio/Plug-Ins/Components/Calabi Hats.component` | — |
| CLAP | `~/Library/Audio/Plug-Ins/CLAP/Calabi Hats.clap` | `C:\Program Files\Common Files\CLAP\` |
| Standalone | `build/CalabiHats_artefacts/Release/Standalone/` | idem |

---

## Estructura

```
CMakeLists.txt              JUCE + clap-juce-extensions, los 4 formatos
Source/Engine.h             DSP de la voz, patrón de 16 pasos, euclidiano
Source/PluginProcessor.*    parámetros, secuenciador, ruteo de modulación, estado
Source/PluginEditor.*       interfaz: knobs con drag & drop, grillas, módulos plegables
Resources/                  logo oficial Calabi Yau
```

## Cómo funciona

- **Sincronía**: el arpegiador sigue el transporte del host (ppq). En standalone corre libre.
- **MIDI**: cualquier nota dispara un hit; C3 (60) es el tono base, cada semitono transpone
  el metal, el click, el hiss y el FM.
- **Modulación**: las fuentes son LANE 1, LANE 2, VEL y RAND. Se resuelven en el momento del
  golpe, igual que en el prototipo, así cada hit suena distinto sin smearing.
- **Estado**: todos los parámetros van por `AudioProcessorValueTreeState` (automatizables),
  y el patrón, las lanes y los ruteos se guardan en el preset del host.

## Pendiente antes de distribuir

- Firmar y notarizar el build de macOS con tu Developer ID (`codesign` + `notarytool`);
  la firma ad-hoc del script sirve solo en tu máquina.
- Instalador: `pkgbuild`/`productbuild` en Mac, Inno Setup en Windows.
