#!/usr/bin/env bash
set -uo pipefail

BIN="./coherence"
TRACES="./tests/benchmark_traces"
OUTDIR="./tests/out_sweep"

# Clean the output folder
rm -rf "$OUTDIR"/*

mkdir -p "$OUTDIR"
shopt -s nullglob

# Common fixed parameters
PROTOCOLS=("MESI" "Dragon" "MOESI") # MESI | Dragon | MOESI
FIXED_CACHE=4096
FIXED_ASSOC=2
FIXED_BLOCK=32

# Sweep definitions
CACHE_SIZES=(1024 2048 4096 8192 16384)
ASSOCS=(1 2 4 8 16)
BLOCKS=(4 8 16 32 64)

run_sweep() {
  local var_name="$1"
  local -n values=$2     # use nameref to refer to array
  local cs="$FIXED_CACHE"
  local a="$FIXED_ASSOC"
  local b="$FIXED_BLOCK"

  echo "=== Running sweep: $var_name ==="
  for f in "$TRACES"/*_0.data; do
    bn=$(basename "$f")
    bm="${bn%_*}"
    for proto in "${PROTOCOLS[@]}"; do
      for val in "${values[@]}"; do
        case "$var_name" in
          cache) cs="$val" ;;
          assoc) a="$val" ;;
          block) b="$val" ;;
        esac
        out="$OUTDIR/${bm}_${proto}_${cs}_${a}_${b}_${var_name}.json"
        echo "== $bm | $proto | CS=$cs A=$a B=$b ($var_name) =="

        if ! "$BIN" "$proto" "$f" "$cs" "$a" "$b" --json > "$out"; then
          echo "Error running $BIN for $bm ($proto, $cs, $a, $b)" >&2
          rm -f "$out"
          continue
        fi
      done
    done
  done
}

# Run each independent sweep
run_sweep "cache" CACHE_SIZES
run_sweep "assoc" ASSOCS
run_sweep "block" BLOCKS
