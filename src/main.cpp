// CS4223 cache coherence simulator entrypoint.
//
// CLI
//   ./coherence <protocol> <input_base_or_any_0.data> <cache_size> <associativity> <block_size> [--json]
//
// <protocol> can be either "MESI" or "DRAGON".
//
// If <input> ends with "_0.data", we auto-resolve _1/_2/_3 in the same folder.
// If it's a base name with no underscore (e.g., "bodytrack"), we try ./tests/benchmark_traces/bodytrack_0..3.data.

#include <iostream>
#include <string>
#include <vector>
#include "cache_sim.hpp"
#include "utils/utils.hpp"

int main(int argc, char *argv[])
{
    // Parse input arguments.
    // Support variable number of arguments (with default values).
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <protocol: MESI|Dragon> <input_base_or_any_0.data> [<cache_size> <associativity> <block_size>] [--json]\n";
        return 2;
    }

    // Parse protocol: accepted: MESI/Dragon.
    std::string protocol = argv[1];

    // Parse file inputs.
    // All 0..3.data input files must be present.
    std::string input = argv[2];
    auto paths = resolve_four(input);

    // Other arguments.
    const int cache_size = std::stoi(argv[3]);
    const int assoc = std::stoi(argv[4]);
    const int block_size = std::stoi(argv[5]);

    // Check for JSON output flag.
    bool json_output = false;
    for (int i = 6; i < argc; i++)
    {
        if (std::string(argv[i]) == "--json")
        {
            json_output = true;
        }
    }

    // Initialize the stats recorder.
    Stats::initialize(cache_size, assoc, block_size, protocol);

    // CacheSim determines which protocol to use,
    // then loads and simulates using the traces provided.
    CacheSim sim(protocol, cache_size, assoc, block_size);
    sim.load_traces(paths);
    sim.run();

    // Output the results.
    Stats::print_results(json_output);

    return 0;
}
