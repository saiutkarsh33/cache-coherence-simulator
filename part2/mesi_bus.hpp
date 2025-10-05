// mesi_bus.hpp — Bus + op enums + accounting

#pragma once
#include <algorithm>
#include <cstdint>

static constexpr int CYCLE_HIT = 1;
static constexpr int CYCLE_MEM_BLOCK_FETCH = 100;
static constexpr int CYCLE_WRITEBACK_DIRTY = 100;
static constexpr int WORD_BYTES = 4;

// Bus operations
enum class BusOp { None, BusRd, BusRdX, BusUpgr }; // MESI only

struct BusTxn {
  BusOp op = BusOp::None;
  uint32_t addr = 0;
  int src_core = -1;
  int data_bytes = 0; // block bytes for c2c/mem fetch; 0 for Upgr
  int duration = 0;   // cycles: mem=100, c2c=2N, upgr=1 (address-only)
};

struct Bus {
  uint64_t free_at = 0;
  uint64_t total_data_bytes = 0;
  uint64_t invalidation_broadcasts = 0; // count BusUpgr + BusRdX that actually invalidate others

  // FCFS schedule; returns finish time
  uint64_t schedule(uint64_t earliest, const BusTxn& t) {
    uint64_t start = std::max(earliest, free_at);
    uint64_t end   = start + (t.duration > 0 ? (uint64_t)t.duration : 0ull);
    free_at = end;
    total_data_bytes += (uint64_t)t.data_bytes;
    if (t.op == BusOp::BusUpgr || t.op == BusOp::BusRdX) {
      // Count one broadcast per op (not per-recipient)
      invalidation_broadcasts++;
    }
    return end;
  }
};
