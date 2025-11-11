#pragma once
#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include "utils/types.hpp"
#include "utils/stats.hpp"

// The forward declaration is necessary here due to a cyclic reference.
class Cache;

// Bus uses FCFS arbitration policy.
// Broadcasts transactions, coordinate responses.
//
// Multiple processor attempt bus transactions simultaneously.
// We assume that multiple bus transactions can take place simultaneously,
// and that the bus is only locked during the first and last cycle
// (only one core can broadcast/recieve in those cycles).
class Bus
{
private:
    // Store reference to the caches for the broadcast.
    std::vector<std::unique_ptr<Cache>> &caches;

    int block_bytes; // Number of bytes for a block for caches using the bus.

    // Indicates until when the bus is busy.
    // Store the cycles at which the bus is exclusive (first and last cycles of a bus request).
    std::unordered_set<u64> exclusive_use;

    // Returns end time after scheduling in FCFS ordering.
    // Requests are serviced in the order in which they arrive.
    u64 request_bus(u64 earliest, u64 duration_cycles)
    {
        // Find first free start cycle.
        u64 start_time = earliest;
        while (exclusive_use.find(start_time) != exclusive_use.end())
        {
            start_time++;
        }
        exclusive_use.insert(start_time);

        // Get first free end cycle.
        u64 end_time = start_time + duration_cycles;
        while (exclusive_use.find(end_time) != exclusive_use.end())
        {
            end_time++;
        }
        exclusive_use.insert(end_time);

        return end_time;
    }

public:
    Bus(std::vector<std::unique_ptr<Cache>> &caches, int block_bytes)
        : caches(caches), block_bytes(block_bytes) {}
    bool trigger_bus_broadcast(int core_id, int bus_transaction_event, CacheLine *cache_line, int num_cores);
    void access_main_memory(int curr_core, u64 duration_cycles);
};
