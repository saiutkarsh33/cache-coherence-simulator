// mesi_cache.hpp — L1 cache + MESI state + snoop handling
#pragma once
#include <vector>
#include <cstdint>
#include <cassert>

#include "mesi_bus.hpp"

enum class MESI
{
    I,
    S,
    E,
    M
};

struct CacheLine
{
    uint32_t tag = 0;
    bool valid = false;
    bool dirty = false;
    MESI st = MESI::I;
    uint64_t lru = 0;
};

struct CacheSet
{
    std::vector<CacheLine> ways;
    explicit CacheSet(int a) : ways(a) {}
};

class L1CacheMESI
{
public:
    L1CacheMESI(int size_b, int assoc, int block_b)
        : size_bytes(size_b), A(assoc), B(block_b)
    {
        assert(size_b > 0 && assoc > 0 && block_b > 0);
        assert((size_b % (assoc * block_b)) == 0);
        S = size_b / (assoc * block_b);
        sets.reserve(S);
        for (int i = 0; i < S; i++)
            sets.emplace_back(A);
    }

    // Address decode
    inline void decode(uint32_t addr, int &index, uint32_t &tag) const
    {
        uint32_t line_addr = addr / B;
        index = (int)(line_addr % S);
        tag = (uint32_t)(line_addr / S);
    }

    // Lookup
    CacheLine *find(int idx, uint32_t tag, int &way)
    {
        auto &v = sets[idx].ways;
        for (int w = 0; w < A; w++)
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
        for (int w = 0; w < A; w++)
            if (!v[w].valid)
            {
                way = w;
                return &v[w];
            }
        int vw = 0;
        uint64_t best = v[0].lru;
        for (int w = 1; w < A; w++)
            if (v[w].lru < best)
            {
                best = v[w].lru;
                vw = w;
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

    SnoopResult on_busrd(uint32_t addr, uint64_t now)
    {
        int idx;
        uint32_t tag;
        decode(addr, idx, tag);
        int w;
        auto *ln = find(idx, tag, w);
        SnoopResult r{};
        if (!ln)
            return r;
        r.had_line = true;
        // If M: supply block (downgrade to S)
        if (ln->st == MESI::M)
        {
            r.supplied_block = true;
            ln->st = MESI::S;
            ln->dirty = false; // ownership lost; memory remains stale until later writeback (standard)
            ln->lru = now;
        }
        else if (ln->st == MESI::E)
        {
            ln->st = MESI::S;
            ln->lru = now;
        }
        else
        {
            // S stays S
            ln->lru = now;
        }
        return r;
    }

    SnoopResult on_busrdx(uint32_t addr, uint64_t now)
    {
        int idx;
        uint32_t tag;
        decode(addr, idx, tag);
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
    SnoopResult on_busupgr(uint32_t addr, uint64_t now)
    {
        int idx;
        uint32_t tag;
        decode(addr, idx, tag);
        int w;
        auto *ln = find(idx, tag, w);
        SnoopResult r{};
        if (!ln)
            return r;
        r.had_line = true;
        if (ln->st == MESI::S)
        {
            ln->valid = false;
            ln->dirty = false;
            ln->st = MESI::I;
            ln->lru = now;
            r.invalidated = true;
        }
        else
        {
            ln->lru = now;
        }
        return r;
    }

    // Processor access. Returns whether hit and the new state target if miss/write.
    struct Access
    {
        bool hit = false;
        bool needs_bus = false;
        bool is_write = false;
        BusOp busop = BusOp::None;
        bool data_from_mem = true; // false -> c2c from M
        // For stats:
        bool eviction_writeback = false;
    };

    Access pr_access(uint64_t tick, bool store, uint32_t addr, int &service_extra_cycles,
                     int &bus_data_bytes, int block_words, bool &upgrade_only)
    {
        // Hit path fast: no bus unless S->M write (needs BusUpgr)
        access_clock++;
        int idx;
        uint32_t tag;
        decode(addr, idx, tag);
        int w;
        auto *ln = find(idx, tag, w);

        service_extra_cycles = 0;
        bus_data_bytes = 0;
        upgrade_only = false;

        if (ln)
        {
            ln->lru = access_clock;
            if (!store)
                return {true, false, false, BusOp::None, true, false};
            // store hit
            if (ln->st == MESI::M)
            {
                ln->dirty = true;
                return {true, false, true, BusOp::None, true, false};
            }
            if (ln->st == MESI::E)
            {
                ln->st = MESI::M;
                ln->dirty = true;
                return {true, false, true, BusOp::None, true, false};
            }
            if (ln->st == MESI::S)
            {
                // Need BusUpgr (address-only). No extra cycles beyond bus; simulator will schedule bus op.
                upgrade_only = true;
                return {true, true, true, BusOp::BusUpgr, true, false};
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
            bus_data_bytes += B;
            wb = true;
        }
        // Fetch block (source decided by simulator after snoops)
        service_extra_cycles += CYCLE_MEM_BLOCK_FETCH; // pessimistic; simulator will replace with 2N if c2c
        bus_data_bytes += B;

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

    // Debug / stats helpers
    int sets_count() const { return S; }
    int block_bytes() const { return B; }

private:
    int size_bytes, A, B, S;
    uint64_t access_clock = 0;
    std::vector<CacheSet> sets;
};
