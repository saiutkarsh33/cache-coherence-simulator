#ifndef TRACE_ITEM_HPP
#define TRACE_ITEM_HPP

#include "types.hpp"

struct TraceItem
{
    // (gap_cycles_before_op, addr, is_mem, is_store)
    u64 gap = 0;
    u32 addr = 0;
    bool is_mem = false;
    bool is_store = false;
};

#endif