#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <iostream>
#include <cstdlib>
#include <sys/stat.h>
#include "types.hpp"

inline bool file_exists(const std::string &path)
{
    struct stat sb{};
    // I_ISREG checks if the mode indicates a regular file.
    return ::stat(path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode);
}

inline u64 parse_auto_base(const std::string &s)
{
    // Accepts "1234", "0x4d2", etc.
    std::size_t idx = 0;
    u64 v = 0;
    try
    {
        v = std::stoull(s, &idx, 0); // base 0 -> auto-detect (0x for hex)
    }
    catch (...)
    {
        std::cerr << "Failed to parse numeric value: '" << s << "'\n";
        std::exit(2);
    }
    if (idx != s.size())
    {
        std::cerr << "Trailing characters in numeric value: '" << s << "'\n";
        std::exit(2);
    }
    return v;
}

inline std::string resolve_part1_trace_path(const std::string &input)
{
    if (file_exists(input))
        return input;
    std::string alt = input + "_0.data";
    if (file_exists(alt))
        return alt;
    std::cerr << "Could not find trace file: '" << input << "' or '" << alt << "'\n";
    std::exit(2);
}

#endif
