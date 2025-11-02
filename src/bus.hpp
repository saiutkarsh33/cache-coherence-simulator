#pragma once
#include <algorithm>
#include <cstdint>
#include "utils/types.hpp"
#include "cache.hpp"

// Bus uses FCFS arbitration policy.
// Broadcasts transactions, coordinate responses.
//
// Multiple processor attempt bus transactions simultaneously.
// We assume only one transaction can take place on the bus at any time.
class Bus
{
private:
    u64 free_at = 0; // Indicates until when the bus is busy.

    // Store reference to the caches for the broadcast.
    std::vector<std::unique_ptr<Cache>> &caches;

    // Returns finish time after scheduling in FCFS ordering.
    // Requests are serviced in the order in which they arrive.
    u64 request_bus(u64 earliest, u64 duration_cycles)
    {
        u64 start = std::max(earliest, free_at);
        u64 end = start + duration_cycles;
        free_at = end;
        return end;
    }

public:
    Bus(std::vector<std::unique_ptr<Cache>> &caches) : caches(caches) {};

    // Handle bus broadcasts and adds to the bus traffic bytes and curr_core's idle cycles.
    //
    // Returns true if the cache line is shared.
    bool trigger_bus_broadcast(int curr_core, int bus_transaction_event, CacheLine *cache_line, int bus_traffic_words)
    {
        int curr_time = Stats::get_exec_cycles(curr_core);
        int ready_time = request_bus(curr_time, 0);
        // Must wait until the bus is available since only one transaction can occupy it at a time.
        Stats::add_idle_cycles(curr_core, ready_time - curr_time);

        bool is_shared = false;
        for (int k = 0; k < NUM_OF_CORES; k++)
        {
            if (k != curr_core)
            {
                is_shared = is_shared || caches[k]->trigger_snoop_event(bus_transaction_event, cache_line->addr);
            }
        }

        if (is_shared)
        {
            curr_time = Stats::get_exec_cycles(curr_core);
            int transfer_cycles = bus_traffic_words * 2; // Sending a cache block with N words takes 2N cycles.
            ready_time = request_bus(curr_time, transfer_cycles);

            // Assume the that bus arbitration/broadcast happens instantaneously,
            // only wait for data transfer which happens only when the cache line has sharers.
            Stats::add_idle_cycles(curr_core, ready_time - curr_time);
            Stats::add_bus_traffic_bytes(bus_traffic_words * WORD_BYTES);
            cache_line->valid = true;
        }

        return is_shared;
    }
};
