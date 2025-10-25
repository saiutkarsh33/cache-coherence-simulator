// stats.hpp contain useful classes for calculating and printing output statistics
// for multiple cores.

#ifndef STATS_HPP
#define STATS_HPP

#include <vector>
#include <iostream>
#include "types.hpp"

struct CoreStats
{
    u64 exec_cycles = 0, compute_cycles = 0, idle_cycles = 0;
    u64 loads = 0, stores = 0, hits = 0, misses = 0;
};

class Stats
{
private:
    std::vector<CoreStats> st;

    // overall_exec is based on the max exec time across all cores.
    u64 overall_exec = 0;

    // overall bus stats.
    int overall_bus_total_data_bytes = -1;
    int overall_bus_invalidation_broadcasts = -1;

    // cache configuration.
    int block_size;
    int cache_size;
    int association;

public:
    Stats(int cache_size, int assoc, int block_size) : cache_size(cache_size),
                                                       association(assoc),
                                                       block_size(block_size)
    {
        int cores = 4;
        st.assign(cores, CoreStats{});
    }

    void set_overall_bus_stats(u64 total_data_bytes, u64 invalidation_broadcasts)
    {
        overall_bus_total_data_bytes = total_data_bytes;
        overall_bus_invalidation_broadcasts = invalidation_broadcasts;
    }
    void set_exec_cycles(int core, u64 exec_cycles)
    {
        st[core].exec_cycles = exec_cycles;
        overall_exec = std::max(overall_exec, exec_cycles);
    }
    void advance_core_time(int core, u64 advance_time)
    {
        st[core].compute_cycles += advance_time;
    }
    void increment_stores(int core)
    {
        st[core].stores++;
    }
    void increment_loads(int core)
    {
        st[core].loads++;
    }
    void increment_hits(int core)
    {
        st[core].hits++;
    }
    void increment_misses(int core)
    {
        st[core].misses++;
    }
    void increment_idle_cycles(int core, int cycles)
    {
        st[core].idle_cycles += cycles;
    }

    void print_results(bool json) const
    {
        if (json)
        {
            std::cout << "{\n";
            std::cout << "  \"overall_execution_cycles\": " << overall_exec << ",\n";
            std::cout << "  \"per_core_execution_cycles\": [" << st[0].exec_cycles << "," << st[1].exec_cycles << "," << st[2].exec_cycles << "," << st[3].exec_cycles << "],\n";
            std::cout << "  \"per_core_compute_cycles\": [" << st[0].compute_cycles << "," << st[1].compute_cycles << "," << st[2].compute_cycles << "," << st[3].compute_cycles << "],\n";
            std::cout << "  \"per_core_loads\": [" << st[0].loads << "," << st[1].loads << "," << st[2].loads << "," << st[3].loads << "],\n";
            std::cout << "  \"per_core_stores\": [" << st[0].stores << "," << st[1].stores << "," << st[2].stores << "," << st[3].stores << "],\n";
            std::cout << "  \"per_core_idle_cycles\": [" << st[0].idle_cycles << "," << st[1].idle_cycles << "," << st[2].idle_cycles << "," << st[3].idle_cycles << "],\n";
            std::cout << "  \"per_core_hits\": [" << st[0].hits << "," << st[1].hits << "," << st[2].hits << "," << st[3].hits << "],\n";
            std::cout << "  \"per_core_misses\": [" << st[0].misses << "," << st[1].misses << "," << st[2].misses << "," << st[3].misses << "],\n";
            std::cout << "  \"bus_data_traffic_bytes\": " << overall_bus_total_data_bytes << ",\n";
            std::cout << "  \"bus_invalidations_or_updates\": " << overall_bus_invalidation_broadcasts << ",\n";
            std::cout << "  \"protocol\": \"MESI\",\n";
            std::cout << "  \"config\": {\"cache_size\": " << cache_size
                      << ", \"associativity\": " << association
                      << ", \"block_size\": " << block_size << "}\n";
            std::cout << "}\n";
        }
        else
        {
            std::cout << "Overall Execution Cycles: " << overall_exec << "\n";
            std::cout << "Per-core execution cycles: [" << st[0].exec_cycles << "," << st[1].exec_cycles << "," << st[2].exec_cycles << "," << st[3].exec_cycles << "]\n";
            std::cout << "Compute cycles per core:   [" << st[0].compute_cycles << "," << st[1].compute_cycles << "," << st[2].compute_cycles << "," << st[3].compute_cycles << "]\n";
            std::cout << "Loads/stores per core:     [" << st[0].loads << "," << st[1].loads << "," << st[2].loads << "," << st[3].loads << "] / ["
                      << st[0].stores << "," << st[1].stores << "," << st[2].stores << "," << st[3].stores << "]\n";
            std::cout << "Idle cycles per core:      [" << st[0].idle_cycles << "," << st[1].idle_cycles << "," << st[2].idle_cycles << "," << st[3].idle_cycles << "]\n";
            std::cout << "Hits/misses per core:      [" << st[0].hits << "," << st[1].hits << "," << st[2].hits << "," << st[3].hits << "] / ["
                      << st[0].misses << "," << st[1].misses << "," << st[2].misses << "," << st[3].misses << "]\n";
            std::cout << "Bus data traffic (bytes):  " << overall_bus_total_data_bytes << "\n";
            std::cout << "Invalidation broadcasts:   " << overall_bus_invalidation_broadcasts << "\n";
        }
    }
};

#endif