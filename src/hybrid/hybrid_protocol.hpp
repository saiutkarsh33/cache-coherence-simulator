#pragma once
#include <unordered_map>
#include <algorithm>
#include "../coherence_protocol.hpp"
#include "../mesi/mesi_protocol.hpp"
#include "../dragon/dragon_protocol.hpp"

enum class HybridState
{
    I,
    S,
    E,
    M,
    Sc,
    Sm
};

// HybridProtocol: threshold-based decision per block address
// to dynamically select between MESI and Dragon.
class HybridProtocol : public CoherenceProtocol
{
private:
    MESIProtocol mesi;
    DragonProtocol dragon;

    std::unordered_map<u32, int> counters; // per-block sharing counter
    int threshold = 2;
    int block_bytes;

public:
    HybridProtocol(int cache_size, int assoc, int block_b)
        : mesi(cache_size, assoc, block_b),
          dragon(cache_size, assoc, block_b),
          block_bytes(block_b)
    {
    }

    void set_threshold(int t) { threshold = t; }

    // Simple heuristic: use Dragon for highly shared lines
    bool use_dragon_for(u32 addr) const
    {
        auto it = counters.find(addr);
        return (it != counters.end() && it->second >= threshold);
    }

    void record_access(u32 addr, bool is_read)
    {
        auto &c = counters[addr];
        if (is_read)
            ++c; // reads indicate sharing
        else
            c = std::max(0, c - 1); // writes reduce sharing
    }

    // Main processor access
    AccessResult pr_access(u64 tick, bool store, u32 addr,
                           int &service_extra_cycles, int &bus_data_bytes,
                           int block_words, bool &upgrade_only) override
    {
        record_access(addr, !store);

        if (use_dragon_for(addr))
        {
            return dragon.pr_access(tick, store, addr, service_extra_cycles,
                                    bus_data_bytes, block_words, upgrade_only);
        }
        else
        {
            return mesi.pr_access(tick, store, addr, service_extra_cycles,
                                  bus_data_bytes, block_words, upgrade_only);
        }
    }

    // Snoop reactions delegate based on protocol
    SnoopResult on_busrd(u32 addr, u64 now) override
    {
        if (use_dragon_for(addr))
            return dragon.on_busrd(addr, now);
        return mesi.on_busrd(addr, now);
    }

    SnoopResult on_busrdx(u32 addr, u64 now) override
    {
        if (use_dragon_for(addr))
            return dragon.on_busrdx(addr, now);
        return mesi.on_busrdx(addr, now);
    }

    SnoopResult on_busupgr(u32 addr, u64 now) override
    {
        if (use_dragon_for(addr))
            return dragon.on_busupgr(addr, now);
        return mesi.on_busupgr(addr, now);
    }

    // Hybrid unified state lookup
    HybridState get_line_state(u32 addr) const
    {
        if (use_dragon_for(addr))
        {
            auto ds = dragon.get_line_state(addr);
            switch (ds)
            {
            case DragonState::I:
                return HybridState::I;
            case DragonState::Sc:
                return HybridState::Sc;
            case DragonState::Sm:
                return HybridState::Sm;
            case DragonState::M:
                return HybridState::M;
            }
        }
        else
        {
            auto ms = mesi.get_line_state(addr);
            switch (ms)
            {
            case MESIState::I:
                return HybridState::I;
            case MESIState::S:
                return HybridState::S;
            case MESIState::E:
                return HybridState::E;
            case MESIState::M:
                return HybridState::M;
            }
        }
        return HybridState::I; // fallback
    }

    // classify for stats
    AccessType classify_access_type(u32 addr) const override
    {
        HybridState s = get_line_state(addr);
        switch (s)
        {
        case HybridState::M:
        case HybridState::E:
            return AccessType::Private;
        case HybridState::S:
        case HybridState::Sc:
        case HybridState::Sm:
            return AccessType::Shared;
        default:
            return AccessType::Invalid;
        }
    }
};
