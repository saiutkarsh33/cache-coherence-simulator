// main.cpp — CS4223 A2 Part 2 (4-core MESI) entrypoint.
// CLI (compatible with Part 1):
//   ./coherence MESI <input_base_or_any_0.data> <cache_size> <associativity> <block_size> [--json]
//
// If <input> ends with "_0.data", we auto-resolve _1/_2/_3 in the same folder.
// If it's a base name with no underscore (e.g., "bodytrack"), we try ./tests/benchmark_traces/bodytrack_0..3.data then CWD.

#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include "../utils/constants.hpp"
#include "mesi_sim.hpp"
#include "../utils/utils.hpp"

int main(int argc, char *argv[])
{
    if (argc < 6)
    {
        std::cerr << "Usage: " << argv[0]
                  << " MESI <input_base_or_any_0.data> <cache_size> <associativity> <block_size> [--json]\n";
        return 2;
    }
    std::string protocol = argv[1];
    if (protocol != "MESI")
    {
        std::cerr << "This binary implements Part 2 MESI. Use 'MESI' as the protocol arg.\n";
        return 2;
    }
    std::string input = argv[2];
    const int cache_size = std::stoi(argv[3]);
    const int assoc = std::stoi(argv[4]);
    const int block_size = std::stoi(argv[5]);
    bool json = false;
    for (int i = 6; i < argc; i++)
        if (std::string(argv[i]) == "--json")
            json = true;

    auto paths = resolve_four(input);

    MESISim sim(cache_size, assoc, block_size);
    sim.load_traces(paths);
    sim.run();
    sim.print_results(json);
    return 0;
}
