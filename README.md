# Cache Coherence Simulator

This is an implementation of a cache coherence simulator using traces in C++.

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

# or with explicit filename
./coherence Dragon ./traces/bodytrack_0.data 4096 2 32 --json
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

### Sample runs

```bash
> ./coherence MESI ./traces/bodytrack_0.data 1024 1 16

Overall Execution Cycles: 75419786
Per-core execution cycles: [75419786]
Compute cycles per core:  [17729254]
Loads/stores per core:    2380720 / 889412
Idle cycles per core:     [54420400]
Hits/misses per core:     2845917 / 424215
Bus data traffic (bytes): 8707264
Invalidations/Updates:    0
Private vs Shared:        3270132 / 0

> ./coherence MESI ./traces/bodytrack_0.data 4096 2 32

Overall Execution Cycles: 42034386
Per-core execution cycles: [42034386]
Compute cycles per core:  [17729254]
Loads/stores per core:    2380720 / 889412
Idle cycles per core:     [21035000]
Hits/misses per core:     3085991 / 184141
Bus data traffic (bytes): 6731200
Invalidations/Updates:    0
Private vs Shared:        3270132 / 0
```
