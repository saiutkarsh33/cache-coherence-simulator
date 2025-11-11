#pragma once
#include "bus.hpp"
#include "cache.hpp"

// Handle bus broadcasts and adds to the bus traffic bytes and curr_core's idle cycles.
//
// Returns true if the cache line is shared.
bool Bus::trigger_bus_broadcast(int curr_core, int bus_transaction_event, CacheLine *cache_line, int bus_traffic_words)
{
    bool is_shared = false;
    for (int k = 0; k < NUM_OF_CORES; k++)
    {
        if (k != curr_core)
        {
            is_shared = is_shared || caches[k]->trigger_snoop_event(bus_transaction_event, cache_line->addr);
        }
    }

    // Handle cache to cache transfer.
    if (is_shared)
    {
        u64 curr_time = Stats::get_exec_cycles(curr_core);

        // The bus transaction is serialized: a 1-cycle exclusive lock is required
        // for the command broadcast/arbitration at the start, and another 1-cycle exclusive lock
        // is required at the end for data reception/synchronization.
        // The multi-cycle data transfer in between can be overlapped (pipelined) by subsequent command broadcasts.
        int transfer_cycles = bus_traffic_words * 2; // Sending a cache block with N words takes 2N cycles.
        u64 ready_time = request_bus(curr_time, transfer_cycles);

        Stats::add_idle_cycles(curr_core, ready_time - curr_time);
        Stats::add_bus_traffic_bytes(bus_traffic_words * WORD_BYTES); // FIXME: does not need to transfer cycles if met.
        cache_line->valid = true;
    }

    return is_shared;
}

// Handles read and write with main memory.
void Bus::access_main_memory(int curr_core, u64 duration_cycles)
{
    u64 curr_time = Stats::get_exec_cycles(curr_core);
    u64 ready_time = request_bus(curr_time, duration_cycles);

    Stats::add_idle_cycles(curr_core, ready_time - curr_time);
    Stats::add_bus_traffic_bytes(block_bytes); // Assume accessing main memory also adds bus traffic.
}
