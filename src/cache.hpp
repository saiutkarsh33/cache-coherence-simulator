#pragma once
#include <vector>
#include <cassert>
#include "bus.cpp"
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
    Bus &bus;

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
    Cache(int size_b, int assoc, int block_b, int curr_core, Bus &bus, CoherenceProtocol *proto)
        : size_bytes(size_b), block_bytes(block_b), assoc(assoc), curr_core(curr_core), bus(bus), protocol(proto)
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
        CacheLine *cache_line = find_line(set_idx, tag);

        if (cache_line == nullptr)
        {
            // Handle miss: need to allocate/evict.
            Stats::incr_miss(curr_core);
            CacheLine *victim = find_victim(set_idx);

            // Check if victim needs writeback
            if (victim->valid && victim->dirty)
            {
                bus.access_main_memory(curr_core, CYCLE_WRITEBACK_DIRTY);
            }

            // Allocate the line for the current address.
            // We will also need to update the victim's state, but this can only be done within the processor_event (since it is protocol specific).
            // LRU is updated later upon completion of the entire processor event.
            victim->tag = tag;
            victim->addr = addr;
            victim->valid = false; // To set valid flag only after fetch, attempting core to core transfer (only if have sharers) first.
            victim->dirty = false; // Reset dirty flag.

            cache_line = victim;
        }
        else
        {
            // Handle hit
            Stats::incr_hit(curr_core);
            Stats::add_exec_cycles(curr_core, CYCLE_HIT);
        }

        // Run processor event:
        int processor_event = protocol->parse_processor_event(is_write, cache_line);
        bool is_shared = protocol->on_processor_event(processor_event, cache_line);
        if (is_shared)
        {
            Stats::incr_shared_access(curr_core);
        }
        else
        {
            Stats::incr_private_access(curr_core);
        }

        if (!cache_line->valid && !is_shared)
        {
            // Must fetch from main memory if not shared.
            bus.access_main_memory(curr_core, CYCLE_MEM_BLOCK_FETCH);
            cache_line->valid = true;
        }

        assert(cache_line->valid);

        // Assume that LRU time is updated on completion of the processor event.
        cache_line->lru = Stats::get_exec_cycles(curr_core);

        return;
    }

    // Handle snoop bus transactions.
    // Returns true if shared (has valid cache line).
    bool trigger_snoop_event(int bus_transaction, u32 addr)
    {
        auto [set_idx, tag] = decode_address(addr);
        CacheLine *line = find_line(set_idx, tag);

        // If invalid, no snoop processing required.
        if (line == nullptr || !line->valid)
            return false;

        protocol->on_snoop_event(bus_transaction, line);

        return true;
    }
};
