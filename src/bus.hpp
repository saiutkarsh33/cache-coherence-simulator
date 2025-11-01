#pragma once
#include <algorithm>
#include <cstdint>
#include "utils/types.hpp"
#include "cache.hpp"

// BusTxn represents a bus transaction.
struct BusTxn
{
    u32 addr = 0;
    int bus_op = 0; // 0 should represent a no op.
    int src_core = -1;
    int data_bytes = 0; // block bytes for reads/writes; 4 bytes for BusUpd; 0 for BusUpgr
    int duration = 0;   // cycles: mem=100, c2c=2N, upd=2, upgr=1
};

// Bus uses FCFS arbitration policy.
// Broadcasts transactions, coordinate responses.
//
// Multiple processor attempt bus transactions simultaneously.
// We assume only one transaction can take place on the bus at any time.
class Bus
{
private:
    u64 free_at = 0;
    u64 total_data_bytes = 0;
    u64 invalidation_or_update_broadcasts = 0;

    // Store reference to the caches.
    std::vector<std::unique_ptr<Cache>> &caches;

public:
    Bus(std::vector<std::unique_ptr<Cache>> &caches) : caches(caches) {};

    // FCFS schedule; returns finish time
    u64 schedule(u64 earliest, const BusTxn &t)
    {
        u64 start = std::max(earliest, free_at);
        u64 end = start + (t.duration > 0 ? (u64)t.duration : 0ull);
        free_at = end;
        total_data_bytes += (u64)t.data_bytes;

        // TODO: Count invalidations or updates on the bus
        // if (t.op == BusOp::BusUpgr || t.op == BusOp::BusRdX || t.op == BusOp::BusUpd)
        // {
        //     invalidation_or_update_broadcasts++;
        // }

        return end;
    }

    // Handle bus broadcasts.
    // Returns true if the cache line is shared.
    bool trigger_bus_broadcast(int curr_core, int bus_transaction_event, u32 addr)
    {
        if (bus_transaction_event == 0) // Skip if No op.
            return false;

        bool is_shared = false;
        for (int k = 0; k < NUM_OF_CORES; k++)
        {
            if (k != curr_core)
            {
                is_shared |= caches[k]->trigger_snoop_event(bus_transaction_event, addr);
            }
        }

        return is_shared;
    }
};
