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

// Simple, line-by-line trace parser (no regex). We coalesce compute (label 2) into gap_before_op.
static std::vector<TraceItem> parse_trace(const std::string &path)
{
    std::ifstream in(path);
    if (!in)
    {
        std::cerr << "Cannot open: " << path << "\n";
        std::exit(2);
    }
    std::vector<TraceItem> out;
    u64 gap = 0;
    std::string label, value;
    while (in >> label >> value)
    {
        int int_label = 0;
        try
        {
            int_label = std::stoi(label);
        }
        catch (...)
        {
            std::cerr << "Bad label in " << path << "\n";
            std::exit(2);
        }

        if (int_label == 2)
        {
            // compute gap
            u64 c = parse_auto_base(value);
            gap += c;
        }
        else if (int_label == 0 || int_label == 1)
        {
            // memory access
            u64 addr64 = parse_auto_base(value);

            TraceItem it;
            it.gap = gap;
            it.is_mem = true;
            it.is_store = (int_label == 1);
            it.addr = (u32)addr64;

            out.push_back(it);
            gap = 0;
        }
        else
        {
            std::cerr << "Invalid label value of " << int_label << " in " << path << "\n";
            std::exit(2);
        }
    }

    // FIXME: should account for trailing compute gap for the case where the input is just compute.
    // trailing compute gap is ignored (no op after it)
    return out;
}

#endif