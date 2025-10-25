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

static inline bool file_exists(const std::string &p)
{
    struct stat sb{};
    return ::stat(p.c_str(), &sb) == 0 && S_ISREG(sb.st_mode);
}

static std::vector<std::string> resolve_four(const std::string &input)
{
    std::vector<std::string> v(4);
    // Case A: explicit _0.data
    if (input.size() > 7 && input.rfind("_0.data") == input.size() - 7)
    {
        const auto base = input.substr(0, input.size() - 7);
        v[0] = base + "_0.data";
        v[1] = base + "_1.data";
        v[2] = base + "_2.data";
        v[3] = base + "_3.data";
        for (auto &p : v)
            if (!file_exists(p))
            {
                std::cerr << "Missing: " << p << "\n";
                std::exit(2);
            }
        return v;
    }
    // Case B: bare base (e.g., "bodytrack") — try ./traces then CWD
    std::vector<std::string> tries{
        DEFAULT_TRACES_PATH + input + "_0.data", DEFAULT_TRACES_PATH + input + "_1.data",
        DEFAULT_TRACES_PATH + input + "_2.data", DEFAULT_TRACES_PATH + input + "_3.data"};
    bool ok = true;
    for (int i = 0; i < 4; i++)
        ok = ok && file_exists(tries[i]);
    if (ok)
        return tries;

    tries = {input + "_0.data", input + "_1.data", input + "_2.data", input + "_3.data"};
    ok = true;
    for (int i = 0; i < 4; i++)
        ok = ok && file_exists(tries[i]);
    if (ok)
        return tries;

    std::cerr << "Could not resolve four trace files for base '" << input << "'.\n";
    std::cerr << "Provide e.g.: ./coherence MESI " << DEFAULT_TRACES_PATH << "bodytrack_0.data 4096 2 32\n ";
    std::exit(2);
}

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
