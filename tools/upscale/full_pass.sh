#!/bin/sh
# The full upscaling pass with the pilot's recipe (upscale.py FINAL_RECIPE, docs/UPSCALE_PLAN.md 6):
#   skin, terrain, button, ui, background: 70% Nomos8k DAT + 30% 4x-UltraSharp
#   fx, shadow: Lanczos; exclude: copied
# at 4x (the master; a 2x tree is made from it). Result: OUT/final (an overlay tree with upscale.json).
#
#   tools/upscale/full_pass.sh [OUT]          default OUT: build/upscale
# Needs the upscale venv (EE_UPSCALE_PY, default toolchain/upscale-venv/bin/python) and the data (EE_DATA_ROOT,
# default <repo>/data).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
OUT=${1:-$REPO/build/upscale}
PY=${EE_UPSCALE_PY:-$REPO/toolchain/upscale-venv/bin/python}
U="$PY -I $HERE/upscale.py"
DATA=${EE_DATA_ROOT:-$REPO/data}

$U --data "$DATA" fetch --only nomos8k_dat,ultrasharp
$U --data "$DATA" run --out "$OUT/models" --methods nomos8k_dat,ultrasharp --scale 4 \
    --category skin,terrain,button,ui,background
$U --data "$DATA" run --out "$OUT/plain" --methods lanczos --scale 4 --category fx,shadow,exclude
$U blend --a "$OUT/models/nomos8k_dat" --b "$OUT/models/ultrasharp" --weight 0.7 --out "$OUT/blend_dat70_us30"
rm -rf "$OUT/final"
$U compose --tree "$OUT/blend_dat70_us30" --tree "$OUT/plain/lanczos" --out "$OUT/final"
