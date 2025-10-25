// main.cpp — CS4223 entrypoint.
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
#include "utils/utils.hpp"
#include "mesi/mesi_sim.hpp"

int main(int argc, char *argv[])
{
    // Parse input arguments.
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <protocol: MESI|Dragon> <input_base_or_any_0.data> [<cache_size> <associativity> <block_size>] [--json]\n";
        return 2;
    }
    std::string protocol = argv[1];
    if (protocol != "MESI" && protocol != "Dragon")
    {
        std::cerr << "Invalid protocol: " << protocol << ".\n"
                  << "Protocol must be MESI or Dragon.\n ";
        return 2;
    }

    // File inputs
    std::string input = argv[2];

    // Other arguments
    const int cache_size = std::stoi(argv[3]);
    const int assoc = std::stoi(argv[4]);
    const int block_size = std::stoi(argv[5]);

    bool json_output = false;
    for (int i = 6; i < argc; i++)
    {
        if (std::string(argv[i]) == "--json")
        {
            json_output = true;
        }
    }

    // Parse the input file.
    // All 0..3.data input files must be present.
    auto paths = resolve_four(input);

    // Determine which protocol to use.
    if (protocol == "MESI")
    {
        MESISim sim(cache_size, assoc, block_size);
        sim.load_traces(paths);
        sim.run();
        sim.print_results(json_output);
        return 0;
    }
    if (protocol == "DRAGON")
    {
        std::cerr << "DRAGON protocol not implemented.\n";
        return 0;
    }

    return 0;
}
