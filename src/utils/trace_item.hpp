#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <charconv>
#include "types.hpp"
#include "utils.hpp"

// Operation defines the possible operations the input can take.
enum class Operation
{
    Load,
    Store,
    Other,
};

static Operation parse_operation_sv(std::string_view label)
{
    int value = 0;
    auto [ptr, ec] = std::from_chars(label.data(), label.data() + label.size(), value);
    if (ec != std::errc{} || ptr != label.data() + label.size())
    {
        std::cerr << "Invalid label value: " << label << "\n";
        std::exit(2);
    }

    switch (value)
    {
    case 0:
        return Operation::Load;
    case 1:
        return Operation::Store;
    case 2:
        return Operation::Other;
    default:
        std::cerr << "Invalid label value: " << label << "\n";
        std::exit(2);
    }
}
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
            op = parse_operation_sv(label);
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
            it.cycles = parse_auto_base_sv(value);
        }
        else
        {
            // memory access
            it.addr = (u32)parse_auto_base_sv(value);
        }

        out.push_back(it);
    }

    return out;
}