// mesi_cache.hpp — L1 cache + MESI state + snoop handling
#pragma once
#include <vector>
#include <cstdint>
#include <cassert>
#include "../utils/types.hpp"
#include "mesi_bus.hpp"

enum class MESI
{
    I, // Invalid
    S, // Shared
    E, // Exclusive
    M, // Modified
};

struct CacheLine
{
    u32 tag = 0;
    bool valid = false;
    bool dirty = false;
    MESI st = MESI::I;
    u64 lru = 0;
};

struct CacheSet
{
    std::vector<CacheLine> ways;
    explicit CacheSet(int a) : ways(a) {}
};

class L1CacheMESI
{
private:
    int size_bytes, assoc, block_bytes, sets_count;
    u64 access_clock = 0;
    std::vector<CacheSet> sets;

public:
    L1CacheMESI(int size_b, int assoc, int block_b)
        : size_bytes(size_b), assoc(assoc), block_bytes(block_b)
    {
        assert(size_b > 0 && assoc > 0 && block_b > 0);
        assert((size_b % (assoc * block_b)) == 0);
        sets_count = size_b / (assoc * block_b);
        sets.reserve(sets_count);
        for (int i = 0; i < sets_count; i++)
        {
            sets.emplace_back(assoc);
        }
    }

    // Lookup and return a pointer to the cache line.
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

    // Victim (invalid first, else LRU)
    CacheLine *victim(int idx, int &way)
    {
        auto &v = sets[idx].ways;
        for (int w = 0; w < assoc; w++)
        {
            if (!v[w].valid)
            {
                way = w;
                return &v[w];
            }
        }
        int vw = 0;
        u64 best = v[0].lru;
        for (int w = 1; w < assoc; w++)
        {
            if (v[w].lru < best)
            {
                best = v[w].lru;
                vw = w;
            }
        }
        way = vw;
        return &v[vw];
    }

    // Snoop reactions:
    // Return flags so simulator knows if this cache supplies data (M owner on BusRd / BusRdX)
    struct SnoopResult
    {
        bool had_line = false;
        bool supplied_block = false; // only from M owner
        bool invalidated = false;
    };

    SnoopResult on_busrd(u32 addr, u64 now)
    {
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);

        int w;
        auto *cache_line = find(idx, tag, w);
        SnoopResult r{};
        if (!cache_line)
        {
            return r;
        }
        r.had_line = true;

        // If M: supply block (downgrade to S)
        if (cache_line->st == MESI::M)
        {
            r.supplied_block = true;
            cache_line->st = MESI::S;
            cache_line->dirty = false; // ownership lost; memory remains stale until later writeback (standard)
            cache_line->lru = now;
        }
        else if (cache_line->st == MESI::E)
        {
            cache_line->st = MESI::S;
            cache_line->lru = now;
        }
        else
        {
            // S stays S
            cache_line->lru = now;
        }
        return r;
    }

    SnoopResult on_busrdx(u32 addr, u64 now)
    {
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        auto *ln = find(idx, tag, w);
        SnoopResult r{};
        if (!ln)
            return r;
        r.had_line = true;
        if (ln->st == MESI::M)
        {
            r.supplied_block = true;
        }
        // All S/E/M -> I
        ln->valid = false;
        ln->dirty = false;
        ln->st = MESI::I;
        ln->lru = now;
        r.invalidated = true;
        return r;
    }

    // Upgrade broadcast (address-only): S->I for others, but that’s handled by simulator via on_busupgr().
    SnoopResult on_busupgr(u32 addr, u64 now)
    {
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        CacheLine *cache_line = find(idx, tag, w);
        SnoopResult r{};
        if (!cache_line)
            return r;
        r.had_line = true;
        if (cache_line->st == MESI::S)
        {
            cache_line->valid = false;
            cache_line->dirty = false;
            cache_line->st = MESI::I;
            cache_line->lru = now;
            r.invalidated = true;
        }
        else
        {
            cache_line->lru = now;
        }
        return r;
    }

    // Processor access. Returns whether hit and the new state target if miss/write.
    struct Access
    {
        bool hit = false;
        bool needs_bus = false;
        bool is_write = false; // true if write, else false if read.
        BusOp busop = BusOp::None;
        bool data_from_mem = true; // false -> c2c from M
        // For stats:
        bool eviction_writeback = false;
    };

    // pr_access represents a processor access event (PrRead or PrWrite).
    Access pr_access(u64 tick, bool store, u32 addr, int &service_extra_cycles,
                     int &bus_data_bytes, int block_words, bool &upgrade_only)
    {
        // Hit path fast: no bus unless S->M write (needs BusUpgr)
        access_clock++;
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        CacheLine *cache_line = find(idx, tag, w);

        service_extra_cycles = 0;
        bus_data_bytes = 0;
        upgrade_only = false;

        if (cache_line)
        {
            cache_line->lru = access_clock;
            if (!store)
                return Access{true, false, false, BusOp::None, true, false};

            // store hit
            if (cache_line->st == MESI::M)
            {
                cache_line->dirty = true;
                return Access{true, false, true, BusOp::None, true, false};
            }
            if (cache_line->st == MESI::E)
            {
                cache_line->st = MESI::M;
                cache_line->dirty = true;
                return Access{true, false, true, BusOp::None, true, false};
            }
            if (cache_line->st == MESI::S)
            {
                // Need BusUpgr (address-only). No extra cycles beyond bus; simulator will schedule bus op.
                upgrade_only = true;
                return Access{true, true, true, BusOp::BusUpgr, true, false};
            }
        }

        // Miss path
        // Choose victim
        int vw;
        auto *vic = victim(idx, vw);
        bool wb = false;
        if (vic->valid && vic->dirty)
        {
            // write back victim later (serialized in our blocking model)
            service_extra_cycles += CYCLE_WRITEBACK_DIRTY;
            bus_data_bytes += block_bytes;
            wb = true;
        }
        // Fetch block (source decided by simulator after snoops)
        service_extra_cycles += CYCLE_MEM_BLOCK_FETCH; // pessimistic; simulator will replace with 2N if c2c
        bus_data_bytes += block_bytes;

        // Fill line
        vic->valid = true;
        vic->tag = tag;
        vic->lru = access_clock;
        if (!store)
        {
            vic->st = MESI::E;
            vic->dirty = false;
        }
        else
        {
            vic->st = MESI::M;
            vic->dirty = true;
        }

        return {false, true, store, store ? BusOp::BusRdX : BusOp::BusRd, true, wb};
    }

    // Called when simulator determines the real source (mem vs c2c) and wants to adjust stats.
    void adjust_fill_after_source(bool from_mem, int &service_extra_cycles, int &bus_data_bytes, int block_words) const
    {
        if (from_mem)
        {
            // already accounted: +100 cycles, +B bytes
            return;
        }
        else
        {
            // Replace mem fetch (100, +B) with c2c (2N cycles, +B). We keep the +B; adjust cycles:
            service_extra_cycles -= CYCLE_MEM_BLOCK_FETCH;
            service_extra_cycles += 2 * block_words;
            // bus_data_bytes already had +B; keep it
        }
    }
};
