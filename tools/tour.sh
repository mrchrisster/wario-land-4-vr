#!/bin/bash
# tools/tour.sh <outdir> <base.state (frame ~800, before the first level loads)> "p l" ...
# For each passage/level: patch the state, load it, let the level start, walk right,
# dump STOCK 3D planes at two points and log the parallax estimate.
cd "$(dirname "$0")/.." || exit 1
OUT=$1; BASE=$2; shift 2
for pl in "$@"; do
  set -- $pl; P=$1; L=$2; D=$OUT/p${P}l${L}; mkdir -p $D/a $D/b
  python3 tools/patch_level.py "$BASE" $D/start.state $P $L
  GBARECOMP_PLANE_DUMP=$D/a GBARECOMP_INPUT_REPLAY=$OUT/walkright.txt tools/run_headless.sh 450 $D/a/shot.png --load-state $D/start.state --dump-state $D/a/s.state > /dev/null 2>&1
  GBARECOMP_PARALLAX_LOG=1 GBARECOMP_PLANE_DUMP=$D/b GBARECOMP_INPUT_REPLAY=$OUT/walkright.txt tools/run_headless.sh 1000 $D/b/shot.png --load-state $D/start.state --dump-state $D/b/s.state 2> $D/parallax.log > $D/run.log
  [ -f recomp_master_misses_AWAE.toml.frag ] && python3 tools/merge_misses.py > /dev/null && rm -f recomp_master_misses_AWAE.toml.frag
  echo "p$P l$L rc=$? $(grep -o 'dispatch_misses=[0-9]*' $D/run.log) $(grep parallax $D/parallax.log | tail -1 | grep -o 'ratio.*')"
done
