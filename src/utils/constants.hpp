#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <string>

// -------------------
// Tunable constants
// -------------------
static constexpr int CYCLE_HIT = 1;               // L1 hit latency (cycles) — given
static constexpr int CYCLE_MEM_BLOCK_FETCH = 100; // fetch a block from memory — given
static constexpr int CYCLE_WRITEBACK_DIRTY = 100; // dirty writeback on eviction — given
static constexpr int WORD_BYTES = 4;              // 4 bytes form a word.

// -------------------
// CLI Defaults
// -------------------
static constexpr int DEFAULT_CACHE_SIZE = 4096; // 4KB cache size
static constexpr int DEFAULT_ASSOCIATIVITY = 2; // 2-way set associative cache
static constexpr int DEFAULT_BLOCK_SIZE = 32;   // 32 byte block size

// -------------------
// Misc.
// -------------------
static const std::string DEFAULT_TRACES_PATH = "./tests/benchmark_traces/"; // Path to where the traces are stored.

#endif
