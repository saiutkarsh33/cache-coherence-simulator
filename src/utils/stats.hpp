#pragma once
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <string>
#include "types.hpp"
#include "trace_item.hpp"

struct CoreStats
{
    u64 exec_cycles = 0;
    u64 compute_cycles = 0;
    u64 idle_cycles = 0;

    u64 loads = 0;
    u64 stores = 0;
    u64 hits = 0;
    u64 misses = 0;

    u64 private_accesses = 0;
    u64 shared_accesses = 0;
};

class Stats
{
private:
    std::vector<CoreStats> st;

    u64 overall_exec = 0;
    u64 overall_bus_total_data_bytes = 0;
    u64 overall_bus_invalidations_or_updates = 0;

    int block_size;
    int cache_size;
    int association;
    std::string protocol_name;

    // ────────────────────────────────
    // Utility: print comma-separated array of per-core values
    template <typename T>
    void print_array_json(const std::string &key, const std::vector<T> &vals, bool comma = true) const
    {
        std::cout << "  \"" << key << "\": [";
        for (size_t i = 0; i < vals.size(); ++i)
        {
            std::cout << vals[i];
            if (i < vals.size() - 1)
            {
                std::cout << ",";
            }
        }
        std::cout << "]" << (comma ? ",\n" : "\n");
    }

    // Utility: extract per-core metric as vector
    template <typename F>
    std::vector<u64> collect_metric(F getter) const
    {
        std::vector<u64> result;
        result.reserve(st.size());
        for (const auto &c : st)
        {
            result.push_back(getter(c));
        }
        return result;
    }

public:
    Stats(int cache_size, int assoc, int block_size, std::string protocol_name)
        : cache_size(cache_size),
          association(assoc),
          block_size(block_size),
          protocol_name(std::move(protocol_name))
    {
        st.assign(4, CoreStats{});
    }

    // ────────────────────────────────
    // Data collection methods
    void set_protocol_name(const std::string &name) { protocol_name = name; }

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

    void increment_hits(int core) { st[core].hits++; }
    void increment_misses(int core) { st[core].misses++; }
    void increment_idle_cycles(int core, int cycles) { st[core].idle_cycles += cycles; }
    void increment_private_access(int core) { st[core].private_accesses++; }
    void increment_shared_access(int core) { st[core].shared_accesses++; }

    const CoreStats &get_core_stats(int core) const { return st[core]; }

    // ────────────────────────────────
    // Output
    void print_results(bool json) const
    {
        if (json)
        {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "{\n";
            std::cout << "  \"overall_execution_cycles\": " << overall_exec << ",\n";

            print_array_json("per_core_execution_cycles", collect_metric([](const CoreStats &c)
                                                                         { return c.exec_cycles; }));
            print_array_json("per_core_compute_cycles", collect_metric([](const CoreStats &c)
                                                                       { return c.compute_cycles; }));
            print_array_json("per_core_loads", collect_metric([](const CoreStats &c)
                                                              { return c.loads; }));
            print_array_json("per_core_stores", collect_metric([](const CoreStats &c)
                                                               { return c.stores; }));
            print_array_json("per_core_idle_cycles", collect_metric([](const CoreStats &c)
                                                                    { return c.idle_cycles; }));
            print_array_json("per_core_hits", collect_metric([](const CoreStats &c)
                                                             { return c.hits; }));
            print_array_json("per_core_misses", collect_metric([](const CoreStats &c)
                                                               { return c.misses; }));
            print_array_json("per_core_private_accesses", collect_metric([](const CoreStats &c)
                                                                         { return c.private_accesses; }));
            print_array_json("per_core_shared_accesses", collect_metric([](const CoreStats &c)
                                                                        { return c.shared_accesses; }));

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

            // Display results for all cores together in table form
            std::cout << std::left
                      << std::setw(6) << "Core"
                      << std::setw(14) << "Exec"
                      << std::setw(14) << "Compute"
                      << std::setw(12) << "Idle"
                      << std::setw(10) << "Loads"
                      << std::setw(10) << "Stores"
                      << std::setw(10) << "Hits"
                      << std::setw(10) << "Misses"
                      << std::setw(14) << "Private"
                      << std::setw(14) << "Shared"
                      << "\n";

            std::cout << std::string(110, '-') << "\n";

            for (int i = 0; i < static_cast<int>(st.size()); ++i)
            {
                const auto &c = st[i];
                std::cout << std::left
                          << std::setw(6) << i
                          << std::setw(14) << c.exec_cycles
                          << std::setw(14) << c.compute_cycles
                          << std::setw(12) << c.idle_cycles
                          << std::setw(10) << c.loads
                          << std::setw(10) << c.stores
                          << std::setw(10) << c.hits
                          << std::setw(10) << c.misses
                          << std::setw(14) << c.private_accesses
                          << std::setw(14) << c.shared_accesses
                          << "\n";
            }
            std::cout << "\n";
        }
    }
};
