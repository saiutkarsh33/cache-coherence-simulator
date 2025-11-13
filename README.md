# Cache Coherence Simulator

This is an implementation of a cache coherence simulator supporting MESI, MOESI, and Dragon protocols using traces, in C++ programming language (C++17).

Refer to the `src/mesi` folder for the instructions for building and running a MESI specific implementation.

## Supported Protocols

- **MESI**: Modified, Exclusive, Shared, Invalid (baseline)
- **MOESI**: Modified, Owned, Exclusive, Shared, Invalid (optimization of MESI)
- **Dragon**: Dragon update-based protocol

### MOESI Protocol

MOESI is an optimization of MESI that adds an "Owned" (O) state. The key improvement is that when a cache with modified data receives a read request from another core, it can transition to the Owned state and share the data **WITHOUT writing back to memory**, reducing bus traffic and latency.

**Benefits:**

- 10-30% reduction in bus traffic for sharing-intensive workloads
- 5-15% reduction in execution cycles for producer-consumer patterns
- Used in AMD Opteron and ARM Cortex-A processors

**Literature:**

- Sweazey & Smith (1986), "A class of compatible cache consistency protocols and their support by the IEEE futurebus", ISCA

## Design Architecture

The simulator uses a modular design with protocol-specific implementations:

```
src/
├── mesi/mesi_protocol.hpp            # MESI protocol implementation
├── moesi/moesi_protocol.hpp          # MOESI protocol implementation (optimization)
├── dragon/dragon_protocol.hpp        # Dragon protocol implementation
├── cache.hpp                         # Cache structure and access logic (protocol independent)
├── bus.hpp & bus.cpp                 # Bus arbitration and transactions
├── protocol_factory.hpp              # Protocol selection
├── cache_sim.hpp                     # Cache simulator
└── main.cpp                          # Entry point into cache simulator
```

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
./coherence <protocol: MESI|MOESI|Dragon> <input_file> <cache_size> <associativity> <block_size> [--json]
```

- "protocol" is MESI, MOESI, or Dragon
- "input_file" is the benchmark name (name of the input file)
- "cache_size": cache size in bytes
- "associativity": associativity of the cache
- "block_size": block_size in bytes

```bash
# Run MESI protocol
./coherence MESI bodytrack 4096 2 32

# Run MOESI protocol (optimized)
./coherence MOESI bodytrack 4096 2 32

# Run Dragon protocol
./coherence Dragon bodytrack 4096 2 32

# Run with explicit filename
./coherence MOESI ./tests/benchmark_traces/bodytrack_0.data 4096 2 32 --json
```

3. Automated test running with traces:

```bash
# Basic testing
make test

# Extensive testing
make sweep
```

## Comparing Protocols

To compare MOESI against MESI:

```bash
# Run both protocols
./coherence MESI bodytrack 4096 2 32 --json > mesi_result.json
./coherence MOESI bodytrack 4096 2 32 --json > moesi_result.json

# Compare results
python3 scripts/compare_moesi.py mesi_result.json moesi_result.json
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

## MOESI vs MESI Performance

MOESI typically shows improvements in:

| Metric                  | MESI (baseline) | MOESI (optimized) | Improvement |
| ----------------------- | --------------- | ----------------- | ----------- |
| Bus Data Traffic        | Baseline        | 10-30% lower      | Better      |
| Execution Cycles        | Baseline        | 5-15% lower       | Better      |
| Producer-Consumer Tasks | Baseline        | Significant gain  | Better      |

_Improvements vary by workload. Sharing-intensive workloads benefit most._

## Assumptions

Processor events:

- When deciding which core to pick for processor events at the same time, ties are broken by picking the smaller core number.

Bus:

- Bus transactions are pipelined; we wait if the bus is busy but allow overlappig bus transactions (with a lock for initial broadcast and final data sync).
- Bus snooping happens instantaneously for other cores, so we only wait until the bus is available.
- Bus invalidations/updates are only counted once per broadcast (doesn't depend on the number of cores which have a valid cache line).

Cache-to-cache data transfers:

- Perform cache-to-cache data transfers if possible (cache line sharers exist), only reading from main memory if no sharers.
- Bus data traffic and idle time waiting for other caches is counted only when cache-to-cache data transfers happen across the bus (i.e. cache line sharers exist).

Statistics:

- LRU time of a cache line is based on when a processor load/store is completed (instead of when it begins).
- Shared data accesses are counted when a cache-to-cache data transfer happens (cache line sharers exists), OR if the cache line is in a shared state after the processor event. (i.e. exclusive processor writes can have shared data accesses if their cache line is invalid/not owned and is able to read from another core.) Otherwise it is treated as a private data access.
- Cache-to-cache transfers take 2N cycles (N = words per block); memory access takes 100 cycles.

Protocol specific:

- MOESI's Owned state maintains dirty data that can be shared without memory writeback unlike MESI.

## Protocol Comparison

| Feature                | MESI      | MOESI    | Dragon |
| ---------------------- | --------- | -------- | ------ |
| States                 | 4         | 5        | 4      |
| Sharing Modified Data  | Writeback | No WB ✓  | Update |
| Bus Traffic (Sharing)  | High      | Low ✓    | High   |
| Invalidation vs Update | Invalid   | Invalid  | Update |
| Production Use         | Common    | AMD, ARM | Rare   |
