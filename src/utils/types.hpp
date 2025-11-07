#pragma once
#include <cstdint>

using u64 = unsigned long long;
using u32 = uint32_t;

struct CacheLine
{
    int state = 0; // Generic protocol-defined state representation. (MESI/Dragon use different enums)

    u64 lru = 0;
    u32 tag = 0;
    u32 addr;
    bool valid = false;
    bool dirty = false;
};
