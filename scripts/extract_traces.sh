#!/usr/bin/env bash

# Create trace directory if not present
mkdir -p ./traces

# Unzip files
for z in ./benchmarks/*.zip; do
  echo "Extracting $z..."
  # Exclude maxOS resource forks.
  unzip -jo "$z" '*.data' -x "__MACOSX/*" -d ./traces
done
