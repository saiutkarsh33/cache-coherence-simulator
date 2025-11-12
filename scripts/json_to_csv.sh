#!/usr/bin/env bash
set -euo pipefail

OUTDIR="./tests/out_sweep"
OUTFILE="results.csv"

# Header line for CSV
echo "Problem,Protocol,Sweep,Cache Size,Associativity,Block Size,Overall Execution Cycles,Bus Data Traffic Bytes,Bus Invalidations,Bus Updates,Execution Cycles,Compute Cycles,Loads,Stores,Idle Cycles,Hits,Misses,Private Accesses,Shared Accesses" > "$OUTFILE"

for f in "$OUTDIR"/*.json; do
  # Example filename: Dragon_blackscholes_4096_2_32_cache.json
  bn=$(basename "$f" .json)
  IFS="_" read -r proto problem cs a b sweep <<< "$bn"

  # Handle malformed filenames
  if [[ -z "$sweep" ]]; then
    echo "Skipping malformed filename: $bn" >&2
    continue
  fi

  # Extract metrics with jq
  jq -r --arg problem "$problem" --arg proto "$proto" --arg sweep "$sweep" \
        --arg cs "$cs" --arg a "$a" --arg b "$b" '
    def safe_avg(x):
      if (x | type) == "array" then (x | add / length)
      else x
      end;

    [
      $problem,
      $proto,
      $sweep,
      $cs,
      $a,
      $b,
      .overall_execution_cycles,
      .bus_data_traffic_bytes,
      .bus_invalidations,
      .bus_updates,
      (safe_avg(.per_core_execution_cycles) | round),
      (safe_avg(.per_core_compute_cycles) | round),
      (safe_avg(.per_core_loads) | round),
      (safe_avg(.per_core_stores) | round),
      (safe_avg(.per_core_idle_cycles) | round),
      (safe_avg(.per_core_hits) | round),
      (safe_avg(.per_core_misses) | round),
      (safe_avg(.per_core_private_accesses) | round),
      (safe_avg(.per_core_shared_accesses) | round)
    ]
    | @csv
  ' "$f" >> "$OUTFILE"
done

echo "CSV written to: $OUTFILE"
