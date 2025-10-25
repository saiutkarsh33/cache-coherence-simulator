// utils.hpp contain useful functions for parsing the input file.

#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <iostream>
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

// resolve_four resolves 4 input trace files.
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
    std::vector<std::string> tries{
        DEFAULT_TRACES_PATH + input + "_0.data",
        DEFAULT_TRACES_PATH + input + "_1.data",
        DEFAULT_TRACES_PATH + input + "_2.data",
        DEFAULT_TRACES_PATH + input + "_3.data",
    };
    bool ok = true;
    for (int i = 0; i < 4; i++)
    {
        ok = ok && file_exists(tries[i]);
    }
    if (ok)
    {
        return tries;
    }

    tries = {
        input + "_0.data",
        input + "_1.data",
        input + "_2.data",
        input + "_3.data",
    };
    ok = true;
    for (int i = 0; i < 4; i++)
    {
        ok = ok && file_exists(tries[i]);
    }
    if (ok)
    {
        return tries;
    }

    std::cerr << "Could not resolve four trace files for base '" << input << "'.\n";
    std::cerr << "Provide e.g.: ./coherence MESI " << DEFAULT_TRACES_PATH << "bodytrack_0.data 4096 2 32\n";
    std::exit(2);
}

#endif
