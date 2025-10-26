// utils.hpp contain useful functions for parsing the input file.
#pragma once
#include <string>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include <vector>
#include "constants.hpp"
#include "types.hpp"

// file_exists checks if a file with the path exists.
inline bool file_exists(const std::string &path)
{
    struct stat sb{};
    // I_ISREG checks if the mode indicates a regular file.
    return ::stat(path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode);
}

// parse_auto_base helps to parse a hexadecimal string.
// Accepts "1234", "0x4d2", etc.
inline u64 parse_auto_base(const std::string &s)
{
    try
    {
        size_t idx;
        u64 v = std::stoull(s, &idx, 0);
        if (idx != s.size())
            throw std::invalid_argument("Trailing characters");
        return v;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Invalid number '" << s << "': " << e.what() << '\n';
        std::exit(2);
    }
}

// decode_addr decodes an address and returns the [index, tag] as a pair.
inline std::pair<int, u32> decode_address(u32 address, int block_bytes, int set_count)
{
    u32 line_addr = address / block_bytes;
    int index = (int)(line_addr % set_count);
    u32 tag = (u32)(line_addr / set_count);
    return {index, tag};
}

// resolve_part1_trace_path resolves 1 input trace file (for part 1).
inline std::string resolve_part1_trace_path(const std::string &input)
{
    if (file_exists(input))
        return input;
    std::string alt = DEFAULT_TRACES_PATH + input + "_0.data";
    if (file_exists(alt))
        return alt;
    std::cerr << "Could not find trace file: '" << input << "' or '" << alt << "'\n";
    std::exit(2);
}

// resolve_four resolves NUM_OF_CORES input trace files.
static std::vector<std::string> resolve_four(const std::string &input)
{
    std::vector<std::string> v(NUM_OF_CORES);
    // Case A: explicit _0.data
    if (input.size() > 7 && input.rfind("_0.data") == input.size() - 7)
    {
        const auto base = input.substr(0, input.size() - 7);
        for (int i = 0; i < NUM_OF_CORES; i++)
        {
            v[i] = base + "_" + std::to_string(i) + ".data";
        }
        for (auto &p : v)
        {
            if (!file_exists(p))
            {
                std::cerr << "Missing: " << p << "\n";
                std::exit(2);
            }
        }
        return v;
    }

    // Case B: bare base (e.g., "bodytrack") — try DEFAULT_TRACES_PATH then CWD
    std::vector<std::string> tries(NUM_OF_CORES);
    bool ok = true;
    for (int i = 0; i < NUM_OF_CORES; i++)
    {
        tries[i] = DEFAULT_TRACES_PATH + input + "_" + std::to_string(i) + ".data",
        ok = ok && file_exists(tries[i]);
    }
    if (ok)
    {
        return tries;
    }

    ok = true;
    for (int i = 0; i < NUM_OF_CORES; i++)
    {
        tries[i] = input + "_" + std::to_string(i) + ".data",
        ok = ok && file_exists(tries[i]);
    }
    if (ok)
    {
        return tries;
    }

    std::cerr << "Could not resolve " << NUM_OF_CORES << " trace files for base '" << input << "'.\n";
    std::cerr << "Provide e.g.: ./coherence MESI " << DEFAULT_TRACES_PATH << "bodytrack_0.data 4096 2 32\n";
    std::exit(2);
}
