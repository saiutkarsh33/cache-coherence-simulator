#!/usr/bin/env bash
set -uo pipefail

BIN="./coherence" 
TRACES="./tests/benchmark_traces"
OUTDIR="./tests/out_sweep"

mkdir -p "$OUTDIR"
shopt -s nullglob

PROTOCOL=("MESI" "Dragon")
CACHE_SIZES=(1024 2048 4096 8192 16384)
ASSOCS=(1 2 4 8 16)
BLOCKS=(16 32 64 128 256)

for f in "$TRACES"/*_0.data; do
  bn=$(basename "$f")
  bm="${bn%_*}"
  for proto in "${PROTOCOL[@]}"; do
    for cs in "${CACHE_SIZES[@]}"; do
      for a in "${ASSOCS[@]}"; do
        for b in "${BLOCKS[@]}"; do
          out="$OUTDIR/${bm}_${cs}_${a}_${b}_${proto}.json"
          echo "== $bm | PROTO=$proto CS=$cs A=$a B=$b =="
          if ! "$BIN" "$proto" "$f" "$cs" "$a" "$b" --json > "$out"; then
            echo "Error running $BIN for $bm ($proto, $cs, $a, $b)" >&2
            rm -f "$out"
            continue
          fi
        done
      done
    done
  done
done
