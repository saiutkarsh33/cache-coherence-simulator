#pragma once
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include "types.hpp"
#include "trace_item.hpp"

struct CoreStats
{
    u64 exec_cycles = 0;    // time at end
    u64 compute_cycles = 0; // cycles spent in computation (non-memory)
    u64 idle_cycles = 0;    // cycles spent waiting for cache/memory

    u64 loads = 0;
    u64 stores = 0;
    u64 hits = 0;
    u64 misses = 0;

    u64 private_accesses = 0; // accesses to private (E/M) blocks
    u64 shared_accesses = 0;  // accesses to shared (S) blocks
};

class Stats
{
private:
    std::vector<CoreStats> st;

    // Overall timing and bus stats
    u64 overall_exec = 0;
    u64 overall_bus_total_data_bytes = 0;
    u64 overall_bus_invalidations_or_updates = 0; // TODO: separate invalidations and updates.

    // Cache config
    int block_size;
    int cache_size;
    int association;

    std::string protocol_name;

public:
    Stats(int cache_size, int assoc, int block_size, std::string protocol_name)
        : cache_size(cache_size),
          association(assoc),
          block_size(block_size),
          protocol_name(protocol_name)
    {
        int cores = 4;
        st.assign(cores, CoreStats{});
    }

    void set_protocol_name(const std::string &name)
    {
        protocol_name = name;
    }

    void set_overall_bus_stats(u64 total_data_bytes, u64 invalidation_or_update_broadcasts)
    {
        overall_bus_total_data_bytes = total_data_bytes;
        overall_bus_invalidations_or_updates = invalidation_or_update_broadcasts;
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

    void increment_mem_op(int core, Operation op)
    {
        if (op == Operation::Store)
            st[core].stores++;
        else if (op == Operation::Load)
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

    void increment_private_access(int core)
    {
        st[core].private_accesses++;
    }

    void increment_shared_access(int core)
    {
        st[core].shared_accesses++;
    }

    // Core-level getters for easy reporting
    const CoreStats &get_core_stats(int core) const { return st[core]; }

    void print_results(bool json) const
    {
        if (json)
        {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "{\n";
            std::cout << "  \"overall_execution_cycles\": " << overall_exec << ",\n";
            std::cout << "  \"per_core_execution_cycles\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].exec_cycles << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"per_core_compute_cycles\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].compute_cycles << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"per_core_loads\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].loads << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"per_core_stores\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].stores << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"per_core_idle_cycles\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].idle_cycles << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"per_core_hits\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].hits << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"per_core_misses\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].misses << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"per_core_private_accesses\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].private_accesses << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"per_core_shared_accesses\": [";
            for (int i = 0; i < 4; ++i)
                std::cout << st[i].shared_accesses << (i < 3 ? "," : "");
            std::cout << "],\n";

            std::cout << "  \"bus_data_traffic_bytes\": " << overall_bus_total_data_bytes << ",\n";
            std::cout << "  \"bus_invalidations_or_updates\": " << overall_bus_invalidations_or_updates << ",\n";

            std::cout << "  \"protocol\": \"" << protocol_name << "\",\n";
            std::cout << "  \"config\": {\"cache_size\": " << cache_size
                      << ", \"associativity\": " << association
                      << ", \"block_size\": " << block_size << "}\n";
            std::cout << "}\n";
        }
        else
        {
            std::cout << "\n=== Simulation Results (" << protocol_name << " Protocol) ===\n";
            std::cout << "Overall Execution Cycles: " << overall_exec << "\n";
            std::cout << "Bus Data Traffic (bytes): " << overall_bus_total_data_bytes << "\n";
            std::cout << "Invalidation/Update Broadcasts: " << overall_bus_invalidations_or_updates << "\n\n";

            for (int i = 0; i < 4; i++)
            {
                const auto &c = st[i];
                std::cout << "Core " << i << ":\n";
                std::cout << "  Execution cycles: " << c.exec_cycles << "\n";
                std::cout << "  Compute cycles:   " << c.compute_cycles << "\n";
                std::cout << "  Idle cycles:      " << c.idle_cycles << "\n";
                std::cout << "  Loads:            " << c.loads << "\n";
                std::cout << "  Stores:           " << c.stores << "\n";
                std::cout << "  Hits:             " << c.hits << "\n";
                std::cout << "  Misses:           " << c.misses << "\n";
                std::cout << "  Private accesses: " << c.private_accesses << "\n";
                std::cout << "  Shared accesses:  " << c.shared_accesses << "\n\n";
            }
        }
    }
};
