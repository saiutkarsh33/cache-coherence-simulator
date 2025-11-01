# Cache Coherence Simulator

This is an implementation of a cache coherence simulator supporting both MESI and DRAGON protocols using traces, in C++ programming language (C++17).

Refer to the `src/mesi` folder for the instructions for building and running a MESI specific implementation.

## Design Architecture

<!-- TODO -->

## Setup

1. Build the cache coherence simulator:

```bash
# Unzip benchmarks
make extract

# Build C++ files
make build
```

2. Run the cache coherence simulator.

The program takes the input file name and cache configurations as arguments.

Usage:

```bash
./coherence <protocol: MESI|Dragon> <input_file> <cache_size> <associativity> <block_size> [--json]
```

- "protocol" is either MESI or Dragon
- "input_file" is the benchmark name (name of the input file)
- "cache_size": cache size in bytes
- "associativity": associativity of the cache
- "block_size": block_size in bytes

```bash
# Run (Part 1 uses only the *_0.data trace file)
# Example: 4 KiB cache, 2-way, 32B blocks (defaults suggested by the spec)
./coherence MESI bodytrack 4096 2 32

# Run with default parameters
./coherence Dragon bodytrack

# Run with explicit filename
./coherence Dragon ./tests/benchmark_traces/bodytrack_0.data 4096 2 32 --json
```

3. Automated test running with traces:

```bash
# Basic testing
make test

# Extensive testing
make sweep
```

## Output

Running the cache coherence simulator gives the following output:

1. Overall Execution Cycles
2. Per-core execution cycles
3. Compute cycles per core
4. Loads/stores per core
5. Idle cycles per core
6. Hits/misses per core
7. Bus data traffic (bytes)
8. Invalidations/Updates
9. Private vs Shared

## Assumptions

- When deciding which core to pick for bus transactions, ties are broken by the pick the smaller core number.
- Bus transactions occur one at a time, we wait if the bus is busy until it is the core's turn to broadcast the bus transaction.
<!-- TODO -->
