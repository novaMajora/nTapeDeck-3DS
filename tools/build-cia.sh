#!/bin/sh
# Builds nTapeDeck.cia (installable with FBI) from the compiled nTapeDeck.elf.
# Needs makerom and bannertool on PATH, or set MAKEROM / BANNERTOOL to their paths.
set -e
cd "$(dirname "$0")/.."
MAKEROM="${MAKEROM:-makerom}"
BANNERTOOL="${BANNERTOOL:-bannertool}"
[ -f nTapeDeck.elf ] || { echo "Run make first (nTapeDeck.elf is missing)"; exit 1; }
mkdir -p build
"$BANNERTOOL" makebanner -i assets/banner.png -a assets/banner.wav -o build/banner.bnr
"$BANNERTOOL" makesmdh -s "nTapeDeck 3DS" -l "MP3 player with retro themes" -p "Nova White" -i icon.png -o build/icon.icn
"$MAKEROM" -f cia -o nTapeDeck.cia -elf nTapeDeck.elf -rsf tools/ntapedeck.rsf \
    -icon build/icon.icn -banner build/banner.bnr -exefslogo -target t
echo "Built nTapeDeck.cia"
