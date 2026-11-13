# Messy Hat Tricks — manual installation

**The quick way:** double-click `Install-macOS.command`, or right-click
`Install-Windows.bat` and choose *Run as administrator*. They copy everything
into place, clear the quarantine flag and restart the Audio Unit registry.

Everything below is the same thing done by hand.

---

## About the macOS warning

This plugin is signed, but not notarized — notarizing needs a paid Apple
Developer account, and this is free software. Nothing is wrong with the files;
macOS simply cannot tell who built them.

Two different things are at work, and only the second is yours to clear:

- **The signature.** On Apple Silicon, code with *no* signature at all will not
  load under any circumstances. Every build here is ad-hoc signed, which
  satisfies that. You do not have to do anything about it.
- **The quarantine flag.** Your browser attaches this to anything downloaded.
  Clearing it is a normal user action — it is how every unnotarized plugin is
  installed, and the installer script does it for you.

If you prefer to do it by hand, the command is under each table below.

For the **standalone app**, macOS may still refuse the first launch. Open
System Settings → Privacy & Security, scroll to the bottom, and click
**Open Anyway** next to the message about Messy Hat Tricks. On macOS 15 and
later this is the only route — right-click → Open no longer works for
unnotarized apps.

On **Windows**, SmartScreen shows an "unknown publisher" notice on the first
run of the standalone: click *More info*, then *Run anyway*.

---

## By hand

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
