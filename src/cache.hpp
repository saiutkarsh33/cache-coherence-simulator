#pragma once
#include <vector>
#include <cassert>
#include "coherence_protocol.hpp"
#include "utils/types.hpp"
#include "utils/constants.hpp"
#include "utils/utils.hpp"
#include "utils/stats.hpp"

// Cache contains methods for accessing a cache line.
class Cache
{
private:
    int size_bytes;
    int block_bytes;
    int assoc;
    int num_sets;
    int curr_core;

    struct CacheSet
    {
        std::vector<CacheLine> cache_lines;
        explicit CacheSet(int a) : cache_lines(a) {}
    };

    CoherenceProtocol *protocol;
    std::vector<CacheSet> sets;

    // find_line returns null if not found.
    CacheLine *find_line(int set_idx, u32 tag)
    {
        auto &lines = sets[set_idx].cache_lines;
        for (int w = 0; w < assoc; w++)
        {
            if (lines[w].valid && lines[w].tag == tag)
            {
                return &lines[w];
            }
        }

        return nullptr;
    }

    CacheLine *find_victim(int set_idx)
    {
        auto &lines = sets[set_idx].cache_lines;

        // First try finding invalid lines.
        for (int w = 0; w < assoc; w++)
        {
            if (!lines[w].valid)
            {
                return &lines[w];
            }
        }

        // No free lines, so must evict LRU line (LRU replacement policy).
        int victim_way = 0;
        u64 oldest = lines[0].lru;
        for (int w = 1; w < assoc; w++)
        {
            if (lines[w].lru < oldest)
            {
                oldest = lines[w].lru;
                victim_way = w;
            }
        }

        return &lines[victim_way];
    }

    // decode_addr decodes an address and returns the [index, tag] as a pair.
    inline std::pair<int, u32> decode_address(u32 addr) const
    {
        u32 block_addr = addr / block_bytes;
        int set = block_addr % num_sets;
        u32 tag = block_addr / num_sets;
        return {set, tag};
    }

public:
    Cache(int size_b, int assoc, int block_b, CoherenceProtocol *proto, int curr_core)
        : size_bytes(size_b), block_bytes(block_b), assoc(assoc), protocol(proto), curr_core(curr_core)
    {
        assert(size_b > 0 && assoc > 0 && block_b > 0);
        assert((size_b % (assoc * block_b)) == 0);

        num_sets = size_b / (assoc * block_b);
        sets.reserve(num_sets);
        for (int i = 0; i < num_sets; i++)
        {
            sets.emplace_back(assoc);
        }
    }

    // Main processor access method.
    //
    // If is_write is true, then operation is a write, else operation is a read.
    void access_processor_cache(bool is_write, u32 addr)
    {
        auto [set_idx, tag] = decode_address(addr);
        CacheLine *line = find_line(set_idx, tag);

        if (line == nullptr)
        {
            // Handle miss: need to allocate/evict.
            Stats::incr_miss(curr_core);
            CacheLine *victim = find_victim(set_idx);

            // Check if victim needs writeback
            if (victim->valid && victim->dirty)
            {
                Stats::add_idle_cycles(curr_core, CYCLE_WRITEBACK_DIRTY);
                Stats::add_bus_traffic_bytes(block_bytes); // Assume writing to main memory also writes to the bus traffic bytes.
            }

            // We only fetch the from main memory block later (after processor event),
            // if not already present in other caches (attempt core-to-core transfer first).
            victim->tag = tag;     // Allocate line for the current address.
            victim->valid = false; // To check valid flag within on_processor_event to determine if should fetch.
            victim->dirty = false; // Reset dirty flag.
            line = victim;
        }
        else
        {
            // Handle hit
            Stats::incr_hit(curr_core);
            Stats::add_exec_cycles(curr_core, CYCLE_HIT);
        }

        // Run processor event:
        int processor_event = protocol->parse_processor_event(is_write, line);
        bool is_shared = protocol->on_processor_event(processor_event, line);
        if (is_shared)
        {
            Stats::incr_shared_access(curr_core);
        }
        else
        {
            Stats::incr_private_access(curr_core);
        }

        // Check if valid, else fetch the block.
        if (!line->valid)
        {
            if (is_shared)
            {
                // Core to core transfer.
                Stats::add_idle_cycles(curr_core, 2 * block_bytes); // Sending a cache block with takes 2N.
                Stats::add_bus_traffic_bytes(block_bytes);
            }
            else
            {
                // Fetch from main memory.
                Stats::add_idle_cycles(curr_core, CYCLE_MEM_BLOCK_FETCH);
                Stats::add_bus_traffic_bytes(block_bytes); // Assume reading from main memory also adds bus traffic.
            }
            line->valid = true;
        }

        // Assume that LRU time is updated on completion of the processor event.
        line->lru = Stats::get_exec_cycles(curr_core);

        return;
    }

    // Handle snoop bus transactions.
    // Returns true if shared.
    bool trigger_snoop_event(int bus_transaction, u32 addr)
    {
        auto [set_idx, tag] = decode_address(addr);
        CacheLine *line = find_line(set_idx, tag);
        if (line == nullptr || !line->valid)
            return false;

        protocol->on_snoop_event(bus_transaction, line);

        // The line can be invalidated after snooping.
        return line->valid;
    }
};
