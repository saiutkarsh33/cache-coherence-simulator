#!/usr/bin/env bash
set -euo pipefail

OUTDIR="./tests/out_sweep"
OUTFILE="results.csv"

# Header line for CSV
echo "Problem,Protocol,Cache Size,Associativity,Block Size,Overall Execution Cycles,Bus Data Traffic Bytes,Bus Invalidations,Bus Updates,Execution Cycles,Compute Cycles,Loads,Stores,Idle Cycles,Hits,Misses,Private Accesses,Shared Accesses" > "$OUTFILE"

for f in "$OUTDIR"/*.json; do
  # Example filename: Dragon_blackscholes_4096_2_32.json
  bn=$(basename "$f" .json)
  IFS="_" read -r problem proto cs a b <<< "$bn"

  # Ensure jq is installed
  jq -r --arg problem "$problem" --arg proto "$proto" --arg cs "$cs" --arg a "$a" --arg b "$b" '
    def safe_avg(x):
      if (x | type) == "array" then (x | add / length)
      else x
      end;

    [
      $problem,
      $proto,
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
