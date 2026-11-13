# Messy Hat Tricks — licence and credits

Messy Hat Tricks is free software: you can redistribute it and/or modify it
under the terms of the **GNU Affero General Public License, version 3**, as
published by the Free Software Foundation. The full text is in `LICENSE`.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the licence for details.

Copyright © 2026 Facundo Martín Melchor — Calabi Yau, Aarhus.

---

## Why the AGPL, and what it asks of you

The plugin is built on the JUCE framework, which is dual-licensed: either the
**AGPLv3** or a paid commercial JUCE licence. This build uses the free tier, so
the whole plugin is AGPLv3 too.

That means anyone you hand a binary to is entitled to the matching source code.
It does **not** mean the repository has to be public: shipping the source
alongside the download satisfies the licence. The release archive therefore
carries `source/` next to the plugin files.

If you would rather keep the source closed, that needs a paid JUCE licence
instead — see <https://juce.com/get-juce/>.

---

## Third-party components

| Component | Licence | Used for |
| --- | --- | --- |
| [JUCE](https://juce.com) 8.0.4 | AGPLv3 / commercial | Framework, plugin wrappers, DSP |
| [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) | MIT | The CLAP build |
| [CLAP](https://github.com/free-audio/clap) | MIT | The CLAP format itself |
| VST3 SDK (via JUCE) | GPLv3 / Steinberg | The VST3 build |

VST3 is a trademark of Steinberg Media Technologies GmbH. Audio Units is a
trademark of Apple Inc. Neither company endorses this plugin.

---

## Artwork

The drum-head character and the player illustration are the work of
**Nico Nigsar**, who created the Calabi Yau visual identity. The artwork is used
here by arrangement with the illustrator and is **not** covered by the AGPLv3
grant above — the licence covers the source code. Anyone forking this project
should replace the artwork or obtain their own permission.

The "MESSY HAT TRICKS" logotype and the Calabi Yau logotype are the project's
own marks.

No sports federation, apparel brand or athlete is associated with, endorses, or
has licensed this plugin. The kit shown on the character is a generic design and
carries no third-party marks.

---

## The sound

Nothing in the engine is sampled. Every hit is synthesised from band-limited
square oscillators, filtered noise and an FM transient, so there is no sample
library to credit and nothing to clear.
