#pragma once
#include <string>

// -------------------
// Tunable constants
// -------------------
static constexpr int CYCLE_HIT = 1;               // L1 hit latency (cycles) — given
static constexpr int CYCLE_MEM_BLOCK_FETCH = 100; // fetch a block from memory — given
static constexpr int CYCLE_WRITEBACK_DIRTY = 100; // dirty writeback on eviction — given
static constexpr int WORD_BYTES = 4;              // word size is 4 bytes.

// -------------------
// CLI Defaults
// -------------------
static constexpr int DEFAULT_CACHE_SIZE = 4096; // 4KB cache size
static constexpr int DEFAULT_ASSOCIATIVITY = 2; // 2-way set associative cache
static constexpr int DEFAULT_BLOCK_SIZE = 32;   // 32 byte block size

// -------------------
// Trace files configurations
// -------------------
static const std::string DEFAULT_TRACES_PATH = "./tests/benchmark_traces/"; // Path to where the traces are stored.
static constexpr int NUM_OF_CORES = 4;                                      // The number of cores in the simulation. Supports 1 to 4.
