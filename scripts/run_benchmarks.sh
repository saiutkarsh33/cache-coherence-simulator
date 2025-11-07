#!/usr/bin/env bash
set -euo pipefail

# --- config you can tweak ---
BIN="./coherence"              # path to your simulator binary
TRACES="./tests/benchmark_traces" # folder containing *_0.data
EXPECTED_TRACES="./tests/expected" # folder to expected output
OUTDIR="./tests/out"           # output folder
CACHE_SIZE=4096                # bytes
ASSOC=2
BLOCK=32
PROTOCOLS="MESI Dragon" # space-separated list of protocols
# ----------------------------

# Create output directory if it doesn't exist
mkdir -p "$OUTDIR"

# Initialize counters
found=0
failed_diff=0

shopt -s nullglob

# Loop over each protocol specified
for PROTOCOL in $PROTOCOLS; do
  echo "Running protocol: $PROTOCOL"

  # Loop over each trace file
  for f in "$TRACES"/*_0.data; do
    found=1
    
    # Extract base names
    bn=$(basename "$f")             # e.g., bodytrack_0.data
    bm="${bn%_*}"                   # e.g., bodytrack
    
    # Output file name: include the protocol
    out="$OUTDIR/${PROTOCOL}_${bm}_${CACHE_SIZE}_${ASSOC}_${BLOCK}.json"
    
    # Expected file name: assumes expected files are named like bm_PROTOCOL.json
    expected="$EXPECTED_TRACES/${PROTOCOL}_${bm}_${CACHE_SIZE}_${ASSOC}_${BLOCK}.json"
    
    # Run the simulator
    # Note: $PROTOCOL is now passed correctly
    "$BIN" "$PROTOCOL" "$f" "$CACHE_SIZE" "$ASSOC" "$BLOCK" --json > "$out"
    
    # Check for expected file and run diff
    if [[ -f "$expected" ]]; then
        # Use diff with -q for quiet (only reports if files differ) and -u for unified (shows differences)
        if ! diff -u "$expected" "$out"; then
          echo "$out FAILED"
          failed_diff=$((failed_diff + 1))
        else
          echo "$out PASSED"
        fi
    else
      echo "$out SKIPPED"
    fi
  done
done

# --- Final Summary ---
echo ""
if [[ "$found" == 0 ]]; then
  echo "No *_0.data traces found in $TRACES"
  exit 1
fi

if [[ "$failed_diff" == 0 ]]; then
  echo "All checks passed!"
  exit 0
else
  echo "$failed_diff check(s) failed."
  exit 1
fi
