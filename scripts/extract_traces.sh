#!/usr/bin/env bash

# Create trace directory if not present
mkdir -p ./tests/benchmark_traces

# Unzip files
for z in ./tests/benchmark/*.zip; do
  echo "Extracting $z..."
  # Exclude maxOS resource forks.
  unzip -jo "$z" '*.data' -x "__MACOSX/*" -d ./tests/benchmark_traces
done
