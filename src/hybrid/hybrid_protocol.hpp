// hybrid_protocol.hpp
#pragma once
#include <unordered_map>
#include "../coherence_protocol.hpp"
#include "../mesi/mesi_protocol.hpp"
#include "../dragon/dragon_protocol.hpp"

// HybridProtocol: threshold-based decision per block address
class HybridProtocol : public CoherenceProtocol
{
private:
    MESIProtocol mesi;
    DragonProtocol dragon;

    // per-block counters (keyed by block tag or full addr — small memory overhead)
    std::unordered_map<u32, int> counters;
    int threshold = 2;

    int block_bytes;

public:
    HybridProtocol(int cache_size, int assoc, int block_b)
        : mesi(cache_size, assoc, block_b), dragon(cache_size, assoc, block_b), block_bytes(block_b) {}

    void set_threshold(int t) { threshold = t; }

    bool should_update(u32 addr)
    {
        auto it = counters.find(addr);
        if (it == counters.end())
            return false;
        return it->second >= threshold;
    }

    void record_access(u32 addr, bool is_read)
    {
        auto &c = counters[addr];
        if (is_read)
            ++c;
        else
            c = std::max(0, c - 1);
    }

    AccessResult pr_access(u64 tick, bool store, u32 addr, int &service_extra_cycles,
                           int &bus_data_bytes, int block_words, bool &upgrade_only) override
    {
        record_access(addr, !store);
        if (should_update(addr))
        {
            return dragon.pr_access(tick, store, addr, service_extra_cycles, bus_data_bytes, block_words, upgrade_only);
        }
        else
        {
            return mesi.pr_access(tick, store, addr, service_extra_cycles, bus_data_bytes, block_words, upgrade_only);
        }
    }

    SnoopResult on_busrd(u32 addr, u64 now) override
    {
        if (should_update(addr))
            return dragon.on_busrd(addr, now);
        return mesi.on_busrd(addr, now);
    }
    SnoopResult on_busrdx(u32 addr, u64 now) override
    {
        if (should_update(addr))
            return dragon.on_busrdx(addr, now);
        return mesi.on_busrdx(addr, now);
    }
    SnoopResult on_busupgr(u32 addr, u64 now) override
    {
        if (should_update(addr))
            return dragon.on_busupgr(addr, now);
        return mesi.on_busupgr(addr, now);
    }
};
