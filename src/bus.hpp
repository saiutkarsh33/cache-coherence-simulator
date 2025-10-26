// bus.hpp  — modified
#pragma once
#include <algorithm>
#include <cstdint>
#include "utils/types.hpp"

// Bus operations for protocols.
enum class BusOp
{
    None,
    BusRd,
    BusRdX,
    BusUpgr,
    BusUpd // Update - Dragon based op
};

struct BusTxn
{
    BusOp op = BusOp::None;
    u32 addr = 0;
    int src_core = -1;
    int data_bytes = 0; // block bytes for c2c/mem fetch; 0 for Upgr
    int duration = 0;   // cycles: mem=100, c2c=2N, upd=2N? upgr=1 (address-only)
};

struct Bus
{
    u64 free_at = 0;
    u64 total_data_bytes = 0;
    u64 invalidation_broadcasts = 0; // count invalidation-like ops & updates/invals

    // FCFS schedule; returns finish time
    u64 schedule(u64 earliest, const BusTxn &t)
    {
        u64 start = std::max(earliest, free_at);
        u64 end = start + (t.duration > 0 ? (u64)t.duration : 0ull);
        free_at = end;
        total_data_bytes += (u64)t.data_bytes;

        // We count upgrades and BusRdX as "invalidation-like" and BusUpd as update-broadcast (increment too)
        if (t.op == BusOp::BusUpgr || t.op == BusOp::BusRdX || t.op == BusOp::BusUpd)
        {
            invalidation_broadcasts++;
        }
        return end;
    }
};
