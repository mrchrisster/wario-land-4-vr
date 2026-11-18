#!/bin/bash
# Play Wario Land 4 on the Mac with the Quest STOCK 3D layers shown as tilted planes.
#   Right-drag: orbit   Mouse wheel: layer spacing   F10: tilted / colour layers / flat
#   F12: reset camera   F1-F9 load / Shift+F1-F9 save state slots
# Usage: tools/mac_layer_view.sh [state file]   (e.g. captures/states/hall_start.state)
cd "$(dirname "$0")/.." || exit 1
EXTRA=()
[ -n "${1:-}" ] && EXTRA=(--load-state "$1")
GBARECOMP_LAYER_VIEW=1 exec ./build-mac/WarioLand4Recomp --no-launcher \
  --rom "${WL4_ROM:-$HOME/code/quest_games/Wario Land 4 (USA, Europe).gba}" "${EXTRA[@]}" variants/wario_land_4/game.toml
