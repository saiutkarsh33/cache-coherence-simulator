// coherence_protocol.hpp
#pragma once
#include <cstdint>
#include "bus.hpp"
#include "utils/types.hpp"

// Generic results returned by snoop calls and processor access
struct SnoopResult
{
    bool had_line = false;
    bool supplied_block = false; // supplied data (cache->cache)
    bool invalidated = false;    // the snoop caused invalidation
};

struct AccessResult
{
    bool hit = false;
    bool needs_bus = false;
    bool is_write = false;
    BusOp busop = BusOp::None;
    bool data_from_mem = true; // pessimistic default; simulator may adjust to c2c
    bool eviction_writeback = false;
};

// Abstract base class for coherence protocol implementations
class CoherenceProtocol
{
public:
    virtual ~CoherenceProtocol() = default;

    // Called by simulator when processor issues an access
    virtual AccessResult pr_access(u64 tick, bool store, u32 addr,
                                   int &service_extra_cycles, int &bus_data_bytes,
                                   int block_words, bool &upgrade_only) = 0;

    // Snooping handlers (simulator will call appropriate handler depending on bus op)
    virtual SnoopResult on_busrd(u32 addr, u64 now) = 0;
    virtual SnoopResult on_busrdx(u32 addr, u64 now) = 0;
    virtual SnoopResult on_busupgr(u32 addr, u64 now) = 0;
    // Some protocols may ignore certain ops (default implementation optional)
};
