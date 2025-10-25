#!/usr/bin/env bash
set -euo pipefail

# --- config you can tweak ---
BIN="./coherence"          # path to your simulator binary
TRACES="./tests/benchmark_traces"         # folder containing *_0.data
CACHE_SIZE=4096            # bytes
ASSOC=2
BLOCK=32
PROTOCOL="MESI"            # ignored in Part 1; kept for CLI parity
OUTDIR="./tests/out"
# ----------------------------

mkdir -p "$OUTDIR"

shopt -s nullglob
found=0
for f in "$TRACES"/*_0.data; do
  bn=$(basename "$f")                 # e.g., bodytrack_0.data
  bm="${bn%_*}"                       # e.g., bodytrack
  out="$OUTDIR/${bm}.json"
  echo "== Running $bm =="
  "$BIN" "$PROTOCOL" "$f" "$CACHE_SIZE" "$ASSOC" "$BLOCK" --json > "$out"
  echo "-> wrote $out"
  found=1
done

if [[ "$found" == 0 ]]; then
  echo "No *_0.data traces found in $TRACES"
  exit 1
fi
