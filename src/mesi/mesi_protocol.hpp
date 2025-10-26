// mesi_protocol.hpp
#pragma once
#include <vector>
#include <cstdint>
#include <cassert>
#include "../bus.hpp"
#include "../coherence_protocol.hpp"
#include "../utils/types.hpp"
#include "../utils/constants.hpp"
#include "../utils/utils.hpp"

// MESI states
enum class MESIState
{
    I,
    S,
    E,
    M
};

struct CacheLine
{
    u64 lru = 0;
    u32 tag = 0;
    bool valid = false;
    bool dirty = false;
    MESIState state = MESIState::I;
};

struct CacheSet
{
    std::vector<CacheLine> ways;
    explicit CacheSet(int a) : ways(a) {}
};

class MESIProtocol : public CoherenceProtocol
{
private:
    int size_bytes, assoc, block_bytes, sets_count;
    u64 access_clock = 0;
    std::vector<CacheSet> sets;

public:
    MESIProtocol(int size_b, int assoc_, int block_b)
        : size_bytes(size_b), assoc(assoc_), block_bytes(block_b)
    {
        assert(size_b > 0 && assoc_ > 0 && block_b > 0);
        assert((size_b % (assoc_ * block_b)) == 0);
        sets_count = size_b / (assoc_ * block_b);
        sets.reserve(sets_count);
        for (int i = 0; i < sets_count; i++)
            sets.emplace_back(assoc_);
    }

    CacheLine *find(int idx, u32 tag, int &way)
    {
        auto &v = sets[idx].ways;
        for (int w = 0; w < assoc; w++)
        {
            auto &ln = v[w];
            if (ln.valid && ln.tag == tag)
            {
                way = w;
                return &v[w];
            }
        }
        way = -1;
        return nullptr;
    }

    CacheLine *victim(int idx, int &way)
    {
        auto &v = sets[idx].ways;
        for (int w = 0; w < assoc; w++)
            if (!v[w].valid)
            {
                way = w;
                return &v[w];
            }
        int vw = 0;
        u64 best = v[0].lru;
        for (int w = 1; w < assoc; w++)
            if (v[w].lru < best)
            {
                best = v[w].lru;
                vw = w;
            }
        way = vw;
        return &v[vw];
    }

    // Snoop handlers
    SnoopResult on_busrd(u32 addr, u64 now) override
    {
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        auto *ln = find(idx, tag, w);
        SnoopResult r{};
        if (!ln)
            return r;
        r.had_line = true;
        if (ln->state == MESIState::M)
        {
            r.supplied_block = true;
            ln->state = MESIState::S;
            ln->dirty = false;
            ln->lru = now;
        }
        else if (ln->state == MESIState::E)
        {
            ln->state = MESIState::S;
            ln->lru = now;
        }
        else
        {
            ln->lru = now;
        }
        return r;
    }

    SnoopResult on_busrdx(u32 addr, u64 now) override
    {
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        auto *ln = find(idx, tag, w);
        SnoopResult r{};
        if (!ln)
            return r;
        r.had_line = true;
        if (ln->state == MESIState::M)
            r.supplied_block = true;
        ln->valid = false;
        ln->dirty = false;
        ln->state = MESIState::I;
        ln->lru = now;
        r.invalidated = true;
        return r;
    }

    SnoopResult on_busupgr(u32 addr, u64 now) override
    {
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        auto *ln = find(idx, tag, w);
        SnoopResult r{};
        if (!ln)
            return r;
        r.had_line = true;
        if (ln->state == MESIState::S)
        {
            ln->valid = false;
            ln->dirty = false;
            ln->state = MESIState::I;
            ln->lru = now;
            r.invalidated = true;
        }
        else
        {
            ln->lru = now;
        }
        return r;
    }

    AccessResult pr_access(u64 tick, bool store, u32 addr, int &service_extra_cycles,
                           int &bus_data_bytes, int block_words, bool &upgrade_only) override
    {
        access_clock++;
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        CacheLine *ln = find(idx, tag, w);

        service_extra_cycles = 0;
        bus_data_bytes = 0;
        upgrade_only = false;

        if (ln)
        {
            ln->lru = access_clock;
            if (!store)
                return AccessResult{true, false, false, BusOp::None, true, false};
            // store hit
            if (ln->state == MESIState::M)
            {
                ln->dirty = true;
                return AccessResult{true, false, true, BusOp::None, true, false};
            }
            if (ln->state == MESIState::E)
            {
                ln->state = MESIState::M;
                ln->dirty = true;
                return AccessResult{true, false, true, BusOp::None, true, false};
            }
            if (ln->state == MESIState::S)
            {
                upgrade_only = true;
                return AccessResult{true, true, true, BusOp::BusUpgr, true, false};
            }
        }

        // Miss path
        int vw;
        auto *vic = victim(idx, vw);
        bool wb = false;
        if (vic->valid && vic->dirty)
        {
            service_extra_cycles += CYCLE_WRITEBACK_DIRTY;
            bus_data_bytes += block_bytes;
            wb = true;
        }
        service_extra_cycles += CYCLE_MEM_BLOCK_FETCH; // pessimistic; later adjusted by simulator if c2c
        bus_data_bytes += block_bytes;

        vic->valid = true;
        vic->tag = tag;
        vic->lru = access_clock;
        if (!store)
        {
            vic->state = MESIState::E;
            vic->dirty = false;
        }
        else
        {
            vic->state = MESIState::M;
            vic->dirty = true;
        }

        return AccessResult{false, true, store, store ? BusOp::BusRdX : BusOp::BusRd, true, wb};
    }

    void adjust_fill_after_source(bool from_mem, int &service_extra_cycles, int &bus_data_bytes, int block_words) const
    {
        if (from_mem)
            return;
        service_extra_cycles -= CYCLE_MEM_BLOCK_FETCH;
        service_extra_cycles += 2 * block_words;
    }
};
