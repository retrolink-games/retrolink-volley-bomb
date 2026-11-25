#!/bin/sh
# Builds the three ROMs into dist/:
#   link-volley.gba           the game
#   link-volley-debug.gba     + Select+L forces a desync on that console (proves the DESYNC screen)
#   link-volley-autoplay.gba  + every console plays itself and the host auto-starts (linked soak test / attract demo)
set -e
cd "$(dirname "$0")/.."
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITARM=${DEVKITARM:-$DEVKITPRO/devkitARM}
export PATH="$DEVKITPRO/tools/bin:$PATH"
python3 tools/make_assets.py
mkdir -p dist
build() {
    rm -rf build/*.o build/*.d 2>/dev/null || true
    rm -f link-volley.elf link-volley.gba
    make -j8 EXTRAFLAGS="$2" >/dev/null
    cp link-volley.gba "dist/$1"
    echo "dist/$1"
}
build retro-link-volley-bomb-debug.gba "-DLV_DEBUG_DESYNC"
build retro-link-volley-bomb-autoplay.gba "-DLV_AUTOPLAY"
build retro-link-volley-bomb.gba ""
