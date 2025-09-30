#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

// -------------------
// Tunable constants
// -------------------
static constexpr int CYCLE_HIT = 1;               // L1 hit latency (cycles) — given
static constexpr int CYCLE_MEM_BLOCK_FETCH = 100; // fetch a block from memory — given
static constexpr int CYCLE_WRITEBACK_DIRTY = 100; // dirty writeback on eviction — given // 32-bit word — given

#endif
