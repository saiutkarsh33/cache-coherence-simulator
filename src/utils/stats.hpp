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

    // We account for private and shared accesses,
    // after checking if the cache line is shared,
    // in our protocol processor event handling logic.
    u64 private_accesses = 0;
    u64 shared_accesses = 0;
};

class Stats
{
private:
    std::vector<CoreStats> st;

    u64 overall_exec = 0;                 // Total wall clock time elasped after running the simulation.
    u64 overall_bus_total_data_bytes = 0; // Assume that we only count actual data being transferred (must have sharers).
    u64 overall_bus_invalidations = 0;    // Count upon BusRdX sent on the bus, assume that invalidation counts even if no sharers.
    u64 overall_bus_updates = 0;          // Count upon BusUpd sent on the bus, assume that update counts even if no sharers.

    int block_size = 0;
    int cache_size = 0;
    int association = 0;
    std::string protocol_name;

    Stats() = default;

    template <typename T>
    void print_array_json(const std::string &key, const std::vector<T> &vals, bool comma = true) const
    {
        std::cout << "  \"" << key << "\": [";
        for (size_t i = 0; i < vals.size(); ++i)
        {
            std::cout << vals[i];
            if (i < vals.size() - 1)
                std::cout << ",";
        }
        std::cout << "]" << (comma ? ",\n" : "\n");
    }

    template <typename F>
    std::vector<u64> collect_metric(F getter) const
    {
        std::vector<u64> result;
        result.reserve(st.size());
        for (const auto &c : st)
            result.push_back(getter(c));
        return result;
    }

    static Stats &instance()
    {
        static Stats s;
        return s;
    }

public:
    Stats(const Stats &) = delete;
    Stats &operator=(const Stats &) = delete;

    // ────────────────────────────────
    // Initialization
    static void initialize(int cache_size_, int assoc_, int block_size_, const std::string &protocol_name_)
    {
        auto &s = instance();
        s.cache_size = cache_size_;
        s.association = assoc_;
        s.block_size = block_size_;
        s.protocol_name = protocol_name_;
        s.st.assign(NUM_OF_CORES, CoreStats{});
    }

    // ────────────────────────────────
    // Core statistics
    static void set_exec_cycles(int core, u64 cycles_to_set)
    {
        instance().st[core].exec_cycles = cycles_to_set;
        instance().overall_exec = std::max(instance().overall_exec, cycles_to_set);
    }
    static u64 get_exec_cycles(int core) { return instance().st[core].exec_cycles; }

    static void add_exec_cycles(int core, u64 cycles_to_add) { set_exec_cycles(core, get_exec_cycles(core) + cycles_to_add); }
    static void add_compute_cycles(int core, u64 cycles_to_add)
    {
        instance().st[core].compute_cycles += cycles_to_add;
        instance().add_exec_cycles(core, cycles_to_add);
    }
    static void add_idle_cycles(int core, u64 cycles_to_add)
    {
        instance().st[core].idle_cycles += cycles_to_add;
        instance().add_exec_cycles(core, cycles_to_add);
    }

    static void incr_load(int core) { instance().st[core].loads++; }
    static void incr_store(int core) { instance().st[core].stores++; }
    static void incr_hit(int core) { instance().st[core].hits++; }
    static void incr_miss(int core) { instance().st[core].misses++; }

    static void incr_private_access(int core) { instance().st[core].private_accesses++; }
    static void incr_shared_access(int core) { instance().st[core].shared_accesses++; }

    // ────────────────────────────────
    // Bus statistics
    static void add_bus_traffic_bytes(u64 bytes) { instance().overall_bus_total_data_bytes += bytes; }
    static void incr_bus_invalidations() { instance().overall_bus_invalidations++; }
    static void incr_bus_updates() { instance().overall_bus_updates++; }

    // ────────────────────────────────
    // Output
    static void print_results(bool json)
    {
        auto &s = instance();
        if (json)
        {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "{\n";
            std::cout << "  \"overall_execution_cycles\": " << s.overall_exec << ",\n";

            s.print_array_json("per_core_execution_cycles", s.collect_metric([](const CoreStats &c)
                                                                             { return c.exec_cycles; }));
            s.print_array_json("per_core_compute_cycles", s.collect_metric([](const CoreStats &c)
                                                                           { return c.compute_cycles; }));
            s.print_array_json("per_core_loads", s.collect_metric([](const CoreStats &c)
                                                                  { return c.loads; }));
            s.print_array_json("per_core_stores", s.collect_metric([](const CoreStats &c)
                                                                   { return c.stores; }));
            s.print_array_json("per_core_idle_cycles", s.collect_metric([](const CoreStats &c)
                                                                        { return c.idle_cycles; }));
            s.print_array_json("per_core_hits", s.collect_metric([](const CoreStats &c)
                                                                 { return c.hits; }));
            s.print_array_json("per_core_misses", s.collect_metric([](const CoreStats &c)
                                                                   { return c.misses; }));
            s.print_array_json("per_core_private_accesses", s.collect_metric([](const CoreStats &c)
                                                                             { return c.private_accesses; }));
            s.print_array_json("per_core_shared_accesses", s.collect_metric([](const CoreStats &c)
                                                                            { return c.shared_accesses; }));

            std::cout << "  \"bus_data_traffic_bytes\": " << s.overall_bus_total_data_bytes << ",\n";
            std::cout << "  \"bus_invalidations\": " << s.overall_bus_invalidations << ",\n";
            std::cout << "  \"bus_updates\": " << s.overall_bus_updates << ",\n";
            std::cout << "  \"protocol\": \"" << s.protocol_name << "\",\n";
            std::cout << "  \"config\": {\"cache_size\": " << s.cache_size
                      << ", \"associativity\": " << s.association
                      << ", \"block_size\": " << s.block_size << "}\n";
            std::cout << "}\n";
        }
        else
        {
            std::cout << "\n=== Simulation Results (" << s.protocol_name << " Protocol) ===\n";
            std::cout << "Overall Execution Cycles: " << s.overall_exec << "\n";
            std::cout << "Bus Data Traffic (bytes): " << s.overall_bus_total_data_bytes << "\n";
            std::cout << "Bus Invalidations: " << s.overall_bus_invalidations << "\n";
            std::cout << "Bus Updates: " << s.overall_bus_updates << "\n\n";

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

            for (int i = 0; i < static_cast<int>(s.st.size()); ++i)
            {
                const auto &c = s.st[i];
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
