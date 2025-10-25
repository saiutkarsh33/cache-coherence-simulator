// main.cpp — CS4223 entrypoint.
// CLI
//   ./coherence <protocol> <input_base_or_any_0.data> <cache_size> <associativity> <block_size> [--json]
//
// <protocol> can be either "MESI" or "DRAGON".
//
// If <input> ends with "_0.data", we auto-resolve _1/_2/_3 in the same folder.
// If it's a base name with no underscore (e.g., "bodytrack"), we try ./traces/bodytrack_0..3.data.

#include <iostream>
#include <string>
#include <vector>
#include "utils/utils.hpp"
#include "mesi/mesi_sim.hpp"

int main(int argc, char *argv[])
{
    // TODO: enhance input argument line.
    // Parse input arguments.
    if (argc < 6)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <protocol> <input_base_or_any_0.data> <cache_size> <associativity> <block_size> [--json]\n";
        return 2;
    }

    std::string protocol = argv[1];
    std::string input = argv[2];
    const int cache_size = std::stoi(argv[3]);
    const int assoc = std::stoi(argv[4]);
    const int block_size = std::stoi(argv[5]);
    bool json = false;
    for (int i = 6; i < argc; i++)
    {
        if (std::string(argv[i]) == "--json")
        {
            json = true;
        }
    }

    // Parse the input file.
    auto paths = resolve_four(input);

    // Determine which protocol to use.
    if (protocol == "MESI")
    {
        MESISim sim(cache_size, assoc, block_size);
        sim.load_traces(paths);
        sim.run();
        sim.print_results(json);
        return 0;
    }
    else if (protocol == "DRAGON")
    {
        std::cerr << "DRAGON protocol not implemented.\n";
        return 2;
    }
    else
    {
        std::cerr << "Invalid protocol " << protocol << ".\n";
        return 2;
    }

    return 0;
}
