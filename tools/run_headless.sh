#!/bin/bash
# tools/run_headless.sh <frames> <out.png> [extra args...]  (env GBARECOMP_INPUT_REPLAY etc. pass through)
cd "$(dirname "$0")/.." || exit 1
N=$1; OUT=$2; shift 2
exec ./build-mac/WarioLand4Recomp --no-launcher --volume 0 --rom "${WL4_ROM:-$HOME/code/quest_games/Wario Land 4 (USA, Europe).gba}" \
  --frames "$N" --dump-png "$OUT" "$@" variants/wario_land_4/game.toml
