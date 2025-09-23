#!/usr/bin/env bash
set -euo pipefail

BIN="../part1/coherence" 
TRACES="../traces"
PROTOCOL="MESI"
OUTDIR="./out_sweep"

mkdir -p "$OUTDIR"
shopt -s nullglob

CACHE_SIZES=(4096 8192 16384)
ASSOCS=(1 2 4)
BLOCKS=(32 64)

for f in "$TRACES"/*_0.data; do
  bn=$(basename "$f")
  bm="${bn%_*}"
  for cs in "${CACHE_SIZES[@]}"; do
    for a in "${ASSOCS[@]}"; do
      for b in "${BLOCKS[@]}"; do
        out="$OUTDIR/${bm}_cs${cs}_a${a}_b${b}.json"
        echo "== $bm | CS=$cs A=$a B=$b =="
        "$BIN" "$PROTOCOL" "$f" "$cs" "$a" "$b" --json > "$out"
      done
    done
  done
done
