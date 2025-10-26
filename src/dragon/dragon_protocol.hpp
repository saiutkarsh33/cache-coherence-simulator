#pragma once
#include <vector>
#include <cstdint>
#include <cassert>
#include "../bus.hpp"
#include "../coherence_protocol.hpp"
#include "../utils/constants.hpp"
#include "../utils/types.hpp"

// Dragon states: I, Sc (shared-clean), Sm (shared-modified), E (exclusive clean), M (modified)
enum class DragonState
{
    I, // Invalid state is used to represent that the cache line is not present, just for internal convenience.
    Sc,
    Sm,
    E,
    M
};

struct DragonLine
{
    u64 lru = 0;
    u32 tag = 0;
    bool valid = false;
    bool dirty = false; // logical dirty flag; Dragon treats Sm differently
    DragonState state = DragonState::I;
};

struct DragonSet
{
    std::vector<DragonLine> ways;
    explicit DragonSet(int a) : ways(a) {}
};

class DragonProtocol : public CoherenceProtocol
{
private:
    int size_bytes, assoc, block_bytes, sets_count;
    u64 access_clock = 0;
    std::vector<DragonSet> sets;

public:
    DragonProtocol(int size_b, int assoc_, int block_b)
        : size_bytes(size_b), assoc(assoc_), block_bytes(block_b)
    {
        assert(size_b > 0 && assoc_ > 0 && block_b > 0);
        assert((size_b % (assoc_ * block_b)) == 0);
        sets_count = size_b / (assoc_ * block_b);
        sets.reserve(sets_count);
        for (int i = 0; i < sets_count; i++)
            sets.emplace_back(assoc_);
    }

    DragonLine *find(int idx, u32 tag, int &way)
    {
        auto &v = sets[idx].ways;
        for (int w = 0; w < assoc; w++)
        {
            if (v[w].valid && v[w].tag == tag)
            {
                way = w;
                return &v[w];
            }
        }
        way = -1;
        return nullptr;
    }

    DragonLine *victim(int idx, int &way)
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

    // Snoop handlers for Dragon:
    SnoopResult on_busrd(u32 addr, u64 now) override
    {
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        auto *ln = find(idx, tag, w);
        SnoopResult r{};
        if (!ln)
            return r;
        r.had_line = true;
        // If M: supply data and transition to Sm (shared-modified)
        if (ln->state == DragonState::M)
        {
            r.supplied_block = true;
            ln->state = DragonState::Sm;
            ln->dirty = true;
            ln->lru = now;
        }
        else if (ln->state == DragonState::E)
        {
            // becomes Sc (shared clean)
            ln->state = DragonState::Sc;
            ln->lru = now;
        }
        else
        {
            ln->lru = now;
        }
        return r;
    }

    // BusRdX is rarely used in Dragon; implement as invalidation (safe)
    SnoopResult on_busrdx(u32 addr, u64 now) override
    {
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        auto *ln = find(idx, tag, w);
        SnoopResult r{};
        if (!ln)
        {
            return r;
        }
        r.had_line = true;
        if (ln->state == DragonState::M)
        {
            r.supplied_block = true;
        }
        // Dragon normally avoids invalidation; BusRdX forces invalidation
        ln->valid = false;
        ln->dirty = false;
        ln->state = DragonState::I;
        ln->lru = now;
        r.invalidated = true;
        return r;
    }

    // BusUpgr not used for Dragon, but we implement for compatibility
    SnoopResult on_busupgr(u32 addr, u64 now) override
    {
        SnoopResult r{};
        return r;
    }

    AccessResult pr_access(u64 tick, bool store, u32 addr, int &service_extra_cycles,
                           int &bus_data_bytes, int block_words, bool &upgrade_only) override
    {
        access_clock++;
        auto [idx, tag] = decode_address(addr, block_bytes, sets_count);
        int w;
        DragonLine *ln = find(idx, tag, w);

        service_extra_cycles = 0;
        bus_data_bytes = 0;
        upgrade_only = false;

        if (ln)
        {
            ln->lru = access_clock;
            if (!store)
                return AccessResult{true, false, false, BusOp::None, true, false};

            // WRITE HIT behavior in Dragon:
            // If in E or M => local write
            if (ln->state == DragonState::M || ln->state == DragonState::E)
            {
                ln->state = DragonState::M;
                ln->dirty = true;
                return AccessResult{true, false, true, BusOp::None, true, false};
            }
            // If in Sc: write requires BusUpd (send update to other sharers) -> becomes Sm
            if (ln->state == DragonState::Sc)
            {
                ln->state = DragonState::Sm;
                ln->dirty = true;
                // Broadcast update with data (we set needs_bus and BusUpd)
                // Data may come from this core only; other caches update their copies.
                return AccessResult{true, true, true, BusOp::BusUpd, true, false};
            }
            // If in Sm (already shared-modified), local write OK (but may still update bus if your variant requires)
            if (ln->state == DragonState::Sm)
            {
                // we choose not to broadcast again
                ln->dirty = true;
                return AccessResult{true, false, true, BusOp::None, true, false};
            }
        }

        // MISS path: allocate & fetch block
        int vw;
        auto *vic = victim(idx, vw);
        bool wb = false;
        if (vic->valid && (vic->state == DragonState::M || vic->dirty))
        {
            service_extra_cycles += CYCLE_WRITEBACK_DIRTY;
            bus_data_bytes += block_bytes;
            wb = true;
        }
        service_extra_cycles += CYCLE_MEM_BLOCK_FETCH;
        bus_data_bytes += block_bytes;

        vic->valid = true;
        vic->tag = tag;
        vic->lru = access_clock;
        if (!store)
        {
            // Read miss:
            vic->state = DragonState::Sc;
            vic->dirty = false;
        }
        else
        {
            // Write miss: fetch and become M (some Dragon variants allow Sm/E decisions)
            vic->state = DragonState::M;
            vic->dirty = true;
        }
        // Use BusRd (reads) and BusRdX (writes) as control; for Dragon writes you might choose BusUpd later.
        return AccessResult{false, true, store, store ? BusOp::BusRdX : BusOp::BusRd, true, wb};
    }

    AccessType classify_access_type(u32 addr) const override
    {
        DragonState s = get_line_state(addr);
        if (s == DragonState::M)
            return AccessType::Private;
        if (s == DragonState::Sm || s == DragonState::Sc)
            return AccessType::Shared;
        return AccessType::Invalid;
    }

    DragonState get_line_state(u32 addr) const
    {
        auto [index, tag] = decode_address(addr, block_bytes, sets_count);
        for (const auto &line : sets[index].ways)
        {
            if (line.valid && line.tag == tag)
                return line.state;
        }
        return DragonState::I;
    }

    void adjust_fill_after_source(bool from_mem, int &service_extra_cycles, int &bus_data_bytes, int block_words) const
    {
        if (from_mem)
        {
            return;
        }
        service_extra_cycles -= CYCLE_MEM_BLOCK_FETCH;
        service_extra_cycles += 2 * block_words;
    }
};
