#!/usr/bin/env bash
#
# Messy Hat Tricks — macOS installer
#
# Double-click this file. It copies the plugins into your user plug-in folders
# and clears the quarantine flag macOS puts on anything downloaded from the web.
#
# Nothing here needs admin rights: everything goes into your own Library.

set -uo pipefail
cd "$(dirname "$0")"

echo
echo "  MESSY HAT TRICKS — installer"
echo "  ============================"
echo

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
CLAP_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
APP_DIR="$HOME/Applications"

mkdir -p "$VST3_DIR" "$AU_DIR" "$CLAP_DIR" "$APP_DIR"

installed=0

install_one() {
    local src="$1" dest="$2" label="$3"
    [ -e "$src" ] || return 0

    # Remove the old copy first: macOS caches a plug-in that failed once, and
    # copying over the top of it leaves pieces of the previous build behind.
    rm -rf "$dest/$(basename "$src")"
    cp -R "$src" "$dest/" || { echo "  ✗ could not copy $label"; return 0; }

    xattr -dr com.apple.quarantine "$dest/$(basename "$src")" 2>/dev/null
    echo "  ✓ $label  →  $dest"
    installed=$((installed + 1))
}

for f in *.vst3;      do install_one "$f" "$VST3_DIR" "VST3";        done
for f in *.component; do install_one "$f" "$AU_DIR"   "Audio Unit";  done
for f in *.clap;      do install_one "$f" "$CLAP_DIR" "CLAP";        done
for f in *.app;       do install_one "$f" "$APP_DIR"  "Standalone";  done

echo

if [ "$installed" -eq 0 ]; then
    echo "  Nothing to install — keep this script in the same folder as the"
    echo "  .vst3, .component, .clap and .app files."
    echo
    read -r -p "  Press return to close. " _
    exit 1
fi

# The Audio Unit registry caches a component that was rejected before, so a
# reinstall only takes effect once the registrar is restarted.
killall -9 AudioComponentRegistrar 2>/dev/null

echo "  Done — $installed item(s) installed."
echo
echo "  Now open your DAW and rescan your plug-in folders."
echo "  Bitwig does not rescan on its own: Settings → Locations → rescan."
echo
echo "  To check the Audio Unit passes Apple's own validator, run:"
echo "      auval -v aumu CbHt CyAu"
echo
read -r -p "  Press return to close. " _
