#ifndef TRACE_ITEM_HPP
#define TRACE_ITEM_HPP

#include "types.hpp"

// Operation defines the possible operations the input can take.
enum Operation : int
{
    Load = 0,
    Store = 1,
    Other = 2,
};

// Each TraceItem represents a single line from the input.
struct TraceItem
{
    Operation op;

    // cycles is present only if the operation is a non mem op (other).
    u64 cycles = 0;

    // addr is present only if the operation is a mem op (store or load).
    u32 addr = 0;
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
    std::string label, value;
    while (in >> label >> value)
    {
        Operation op;
        try
        {
            switch (std::stoi(label))
            {
            case 0:
                op = Operation::Load;
                break;
            case 1:
                op = Operation::Store;
                break;
            case 2:
                op = Operation::Other;
                break;
            default:
                std::cerr << "Invalid label value of " << label << " in " << path << "\n";
                std::exit(2);
            }
        }
        catch (...)
        {
            std::cerr << "Bad label in " << path << "\n";
            std::exit(2);
        }

        TraceItem it;
        it.op = op;
        if (op == Operation::Other)
        {
            it.cycles = parse_auto_base(value);
        }
        else
        {
            // memory access
            it.addr = (u32)parse_auto_base(value);
        }

        out.push_back(it);
    }

    return out;
}

#endif
