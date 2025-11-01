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
    out.reserve(1'000'000); // optional

    std::string line;
    while (std::getline(in, line))
    {
        // trim trailing spaces and carriage return
        line.erase(line.find_last_not_of(" \r\n") + 1);
        if (line.empty())
            continue;

        auto pos = line.find_first_of(" \t");
        if (pos == std::string::npos)
        {
            std::cerr << "Bad line in " << path << ": '" << line << "'\n";
            std::exit(2);
        }

        std::string_view label(line.data(), pos);
        std::string_view value(line.data() + pos + 1, line.size() - pos - 1);

        Operation op = parse_operation_sv(label);

        TraceItem it;
        it.op = op;
        u32 parsed_val = parse_auto_base_sv(value);
        if (op == Operation::Other)
            it.cycles = parsed_val;
        else
            it.addr = parsed_val;

        out.push_back(it);
    }

    return out;
}
