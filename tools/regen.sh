#!/bin/bash
# Regenerate C++ from the ROM + game.toml, then rebuild the Mac executable.
cd "$(dirname "$0")/.." || exit 1
RECOMP=${GBA_RECOMPILE:-../Mother3Recomp/build-mac/gbarecomp_build/gba_recompile}
if [ ! -x "$RECOMP" ]; then  # fresh clone: build the recompiler from the submodule runtime
  (cd build-mac && make -j8 gba_recompile >/dev/null) && RECOMP=build-mac/gbarecomp_build/gba_recompile
fi
"$RECOMP" --rom variants/wario_land_4/roms/wario_land_4.gba \
  --config variants/wario_land_4/game.toml --out variants/wario_land_4/generated 2>&1 | tail -3
cd build-mac && make -j14 WarioLand4Recomp 2>&1 | grep -E "error|Built target WarioLand4"
