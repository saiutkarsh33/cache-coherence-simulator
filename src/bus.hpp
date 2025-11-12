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
// and that the bus is only locked during the first and last cycle,
// for pipelining the bus transactions.
// (only one core can broadcast/recieve in those cycles).
class Bus
{
private:
    // Store reference to the caches for the broadcast.
    std::vector<std::unique_ptr<Cache>> &caches;

    int block_bytes; // Number of bytes for a block for caches using the bus.

    // Indicates until when the bus is busy.
    // Store the cycles at which the bus is exclusive (first and last cycles of a bus request).
    std::unordered_set<u64> command_exclusive, data_exclusive;

    // Returns end time after scheduling in FCFS ordering.
    // Requests are serviced in the order in which they arrive.
    //
    // We assume that the duration is extended if not able to acquire the
    // data transfer exclusive lock (instead of stalling the start time to ensure a fixed duration).
    //
    // The request duration is at least one cycle for the exclusive command bus broadcast.
    u64 request_bus(u64 earliest, u64 duration_cycles, bool has_data = false)
    {
        // Find first free start cycle (to acquire a command broadcast lock)
        u64 start_time = earliest;
        while (command_exclusive.find(start_time) != command_exclusive.end())
        {
            start_time++;
        }
        command_exclusive.insert(start_time);

        // Get first free end cycle.
        // Assume that we should add command lock time of 1 cycle.
        u64 end_time = start_time + duration_cycles + 1;
        if (has_data)
        {
            // Assume that we should add data lock time of 1 cycle if data is transfered.
            end_time++; // to acquire a data transfer lock
            while (data_exclusive.find(end_time) != data_exclusive.end())
            {
                end_time++;
            }
            data_exclusive.insert(end_time);
        }

        return end_time;
    }

public:
    Bus(std::vector<std::unique_ptr<Cache>> &caches, int block_bytes)
        : caches(caches), block_bytes(block_bytes) {}
    bool trigger_bus_broadcast(int curr_core, int bus_transaction_event, CacheLine *cache_line, int num_cores);
    void access_main_memory(int curr_core, u64 duration_cycles);
};
