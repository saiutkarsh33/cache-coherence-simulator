// coherence.cpp — CS4223 A2 Part 1 (single-core) in C++17
// Implements: blocking L1 D-cache (WB/WA, LRU), DRAM latency, bus DATA traffic accounting.
// CLI matches assignment: coherence <protocol> <input_file> <cache_size> <associativity> <block_size> [--json]
// Protocol arg is accepted for CLI parity; Part 1 logic is protocol-agnostic by design.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include "../types.hpp"
#include "../constants.hpp"
#include "../utils.hpp"
#include "cache.cpp"
#include "../stats.hpp"

// simulate_single_core just simulates the cache for part 1.
static void simulate_single_core(const std::string &trace_path,
                                 int cache_size, int assoc, int block_size,
                                 bool json_output)
{
    L1Cache cache(cache_size, assoc, block_size);
    CoreStats stats;
    u64 t = 0; // cycle time

    std::ifstream in(trace_path);
    if (!in)
    {
        std::cerr << "Failed to open trace file: " << trace_path << "\n";
        std::exit(2);
    }

    std::string lab_s, val_s;
    while (in >> lab_s >> val_s)
    {
        int lab = 0;
        try
        {
            lab = std::stoi(lab_s);
        }
        catch (...)
        {
            std::cerr << "Bad label in trace: '" << lab_s << "'\n";
            std::exit(2);
        }
        u64 val = parse_auto_base(val_s);

        if (lab == 2)
        {
            // compute (other instructions): advance time
            stats.compute_cycles += val;
            t += val;
            continue;
        }

        // memory op
        int op = lab; // 0=load, 1=store
        if (op == 0)
            ++stats.loads;
        else
            ++stats.stores;

        auto res = cache.access(op, static_cast<u32>(val)); // addresses are 32-bit per spec

        // service time: 1 on hit; 1 + extra on miss
        int service = CYCLE_HIT + res.extra_cycles;
        if (service > CYCLE_HIT)
            stats.idle_cycles += (service - CYCLE_HIT);
        t += service;
    }

    stats.exec_cycles = t;
    stats.hits = cache.hits();
    stats.misses = cache.misses();
    stats.private_accesses = cache.private_accesses();
    stats.shared_accesses = cache.shared_accesses();
    const u64 bus_data_bytes = cache.bus_bytes();
    const u64 inv_or_updates = cache.invalid_or_updates();

    if (json_output)
    {
        // Minimal JSON without external deps.
        std::cout << std::fixed;
        std::cout << "{\n";
        std::cout << "  \"overall_execution_cycles\": " << stats.exec_cycles << ",\n";
        std::cout << "  \"per_core_execution_cycles\": [" << stats.exec_cycles << "],\n";
        std::cout << "  \"per_core_compute_cycles\": [" << stats.compute_cycles << "],\n";
        std::cout << "  \"per_core_loads\": [" << stats.loads << "],\n";
        std::cout << "  \"per_core_stores\": [" << stats.stores << "],\n";
        std::cout << "  \"per_core_idle_cycles\": [" << stats.idle_cycles << "],\n";
        std::cout << "  \"per_core_hits\": [" << stats.hits << "],\n";
        std::cout << "  \"per_core_misses\": [" << stats.misses << "],\n";
        std::cout << "  \"bus_data_traffic_bytes\": " << bus_data_bytes << ",\n";
        std::cout << "  \"bus_invalidations_or_updates\": " << inv_or_updates << ",\n";
        std::cout << "  \"private_accesses\": [" << stats.private_accesses << "],\n";
        std::cout << "  \"shared_accesses\": [" << stats.shared_accesses << "],\n";
        std::cout << "  \"config\": {\"cache_size\": " << cache_size
                  << ", \"associativity\": " << assoc
                  << ", \"block_size\": " << block_size << "}\n";
        std::cout << "}\n";
    }
    else
    {
        std::cout << "Overall Execution Cycles: " << stats.exec_cycles << "\n";
        std::cout << "Per-core execution cycles: [" << stats.exec_cycles << "]\n";
        std::cout << "Compute cycles per core:  [" << stats.compute_cycles << "]\n";
        std::cout << "Loads/stores per core:    " << stats.loads << " / " << stats.stores << "\n";
        std::cout << "Idle cycles per core:     [" << stats.idle_cycles << "]\n";
        std::cout << "Hits/misses per core:     " << stats.hits << " / " << stats.misses << "\n";
        std::cout << "Bus data traffic (bytes): " << bus_data_bytes << "\n";
        std::cout << "Invalidations/Updates:    " << inv_or_updates << "\n";
        std::cout << "Private vs Shared:        " << stats.private_accesses
                  << " / " << stats.shared_accesses << "\n";
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <protocol: MESI|Dragon> <input_file> [<cache_size> <associativity> <block_size>] [--json]\n";
        return 2;
    }

    std::string protocol = argv[1]; // accepted but not used in Part 1
    if (protocol != "MESI" && protocol != "Dragon")
    {
        std::cerr << "Protocol must be MESI or Dragon.\n";
        return 2;
    }

    // File inputs
    std::string input = argv[2];
    std::string trace_path = resolve_part1_trace_path(input);

    // Other arguments
    int cache_size = argc < 4 ? DEFAULT_CACHE_SIZE : std::stoi(argv[3]);
    int assoc = argc < 5 ? DEFAULT_ASSOCIATIVITY : std::stoi(argv[4]);
    int block_size = argc < 6 ? DEFAULT_BLOCK_SIZE : std::stoi(argv[5]);

    bool json_output = false;
    for (int i = 6; i < argc; ++i)
    {
        std::string flag = argv[i];
        if (flag == "--json")
            json_output = true;
    }

    simulate_single_core(trace_path, cache_size, assoc, block_size, json_output);
    return 0;
}
