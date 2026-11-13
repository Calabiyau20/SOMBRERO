# Messy Hat Tricks — manual installation

No installer: copy the files into the folders below and rescan in your host.

## macOS

| Format | Where it goes |
| --- | --- |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| Audio Unit | `~/Library/Audio/Plug-Ins/Components/` |
| CLAP | `~/Library/Audio/Plug-Ins/CLAP/` |
| Standalone | anywhere — `/Applications` is the usual place |

Downloads arrive with a quarantine flag, so clear it once in the Terminal
**before** opening your host:

```bash
xattr -dr com.apple.quarantine \
  ~/Library/Audio/Plug-Ins/VST3/"Messy Hat Tricks.vst3" \
  ~/Library/Audio/Plug-Ins/Components/"Messy Hat Tricks.component" \
  ~/Library/Audio/Plug-Ins/CLAP/"Messy Hat Tricks.clap"
```

Replacing an earlier build? Delete the old one first and clear the Audio Unit
cache, or macOS keeps serving the version it already rejected:

```bash
killall -9 AudioComponentRegistrar
auval -v aumu CbHt CyAu     # Apple's validator: this must reach the end
```

## Windows

| Format | Where it goes |
| --- | --- |
| VST3 | `C:\Program Files\Common Files\VST3\` |
| CLAP | `C:\Program Files\Common Files\CLAP\` |
| Standalone | anywhere |

## Host notes

- **Bitwig** does not rescan by itself: Settings → Locations → rescan.
- **Ableton Live** remembers a plugin that failed once. If it stays greyed out,
  quit Live, delete its plugin database folder, and start it again.
- **MIDI out**: put the plugin on a MIDI track and route its output to whatever
  you want it to play. In Bitwig the device's note output appears directly.

## Presets

Saved as `.messyhat` files in
`~/Documents/Calabi Yau/Messy Hat Tricks/Presets` (macOS) or
`Documents\Calabi Yau\Messy Hat Tricks\Presets` (Windows). Each one holds the
whole instrument: the voice, the sixteen steps, both modulation lanes and every
routing.

## Requirements

- macOS 10.13 or later, Apple Silicon or Intel (one universal build)
- Windows 10 or later, 64-bit
