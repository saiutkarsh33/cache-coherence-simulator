#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include "bus.hpp"
#include "cache.hpp"
#include "protocol_factory.hpp"
#include "utils/trace_item.hpp"
#include "utils/stats.hpp"
#include "utils/types.hpp"
#include "utils/constants.hpp"

class CacheSim
{
private:
    int block_bytes;
    int words_per_block;
    int cache_size;
    int assoc;

    std::vector<std::vector<TraceItem>> traces;

    Bus bus;
    std::vector<std::unique_ptr<Cache>> caches;
    std::vector<size_t> cur_idx;

    // Finds the next core to process,
    // auto advancing through compute operations.
    int find_ready_memop_core()
    {
        int next_core = -1;
        u64 next_time = UINT64_MAX;
        for (int c = 0; c < NUM_OF_CORES; c++)
        {
            u64 max_idx = (u64)traces[c].size();

            // Advance through compute operations (Operation::Other)
            while (cur_idx[c] < max_idx && traces[c][cur_idx[c]].op == Operation::Other)
            {
                auto trace_item = traces[c][cur_idx[c]];
                Stats::add_compute_cycles(c, trace_item.cycles);
                cur_idx[c]++;
            }

            if (cur_idx[c] < max_idx && Stats::get_exec_cycles(c) < next_time)
            {
                next_time = Stats::get_exec_cycles(c);
                next_core = c;
            }
        }
        return next_core;
    }

public:
    CacheSim(const std::string &protocol_name, int cache_size, int assoc, int block_size)
        : block_bytes(block_size),
          words_per_block(block_size / WORD_BYTES),
          cache_size(cache_size),
          assoc(assoc),
          bus(caches)
    {
        assert(block_bytes > 0 && (block_bytes % WORD_BYTES) == 0);

        cur_idx.assign(NUM_OF_CORES, 0);
        caches.reserve(NUM_OF_CORES);
        for (int i = 0; i < NUM_OF_CORES; ++i)
        {
            caches.push_back(make_cache(protocol_name, cache_size, assoc, block_size, i, bus));
        }
    }

    void load_traces(const std::vector<std::string> &paths)
    {
        if (paths.size() != NUM_OF_CORES)
        {
            std::cerr << "need " << NUM_OF_CORES << " traces\n";
            std::exit(2);
        }
        traces.resize(NUM_OF_CORES);
        for (int c = 0; c < NUM_OF_CORES; c++)
        {
            traces[c] = parse_trace(paths[c]);
        }
    }

    void run()
    {
        while (true)
        {
            // Find next core with memory operation to process:
            const int curr_core = find_ready_memop_core();
            if (curr_core < 0)
                break;

            // Process memory operation:
            TraceItem trace_item = traces[curr_core][cur_idx[curr_core]];
            if (trace_item.op == Operation::Load)
                Stats::incr_load(curr_core);
            else if (trace_item.op == Operation::Store)
                Stats::incr_store(curr_core);

            // Access cache:
            caches[curr_core]->access_processor_cache(
                trace_item.op == Operation::Store,
                trace_item.addr);
        }
    }
};
