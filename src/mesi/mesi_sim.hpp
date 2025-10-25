// mesi_sim.hpp — 4-core MESI simulator (discrete-event, blocking caches)
#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <queue>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstdint>
#include "mesi_bus.hpp"
#include "mesi_cache.hpp"
#include "../utils/trace_item.hpp"
#include "../utils/stats.hpp"

class MESISim
{
private:
    int B;       // block bytes
    int N_words; // words per block
    Bus overall_bus;
    L1CacheMESI caches[4];

    Stats stats;

    std::vector<std::vector<TraceItem>> tr;
    std::vector<uint64_t> ready_at;
    std::vector<size_t> cur_idx;

public:
    MESISim(int cache_size, int assoc, int block_size)
        : B(block_size), N_words(block_size / WORD_BYTES),
          caches{L1CacheMESI(cache_size, assoc, block_size),
                 L1CacheMESI(cache_size, assoc, block_size),
                 L1CacheMESI(cache_size, assoc, block_size),
                 L1CacheMESI(cache_size, assoc, block_size)},
          stats(cache_size, assoc, block_size)
    {
        assert(B > 0 && (B % WORD_BYTES) == 0);
    }

    void load_traces(const std::vector<std::string> &paths)
    {
        if (paths.size() != 4)
        {
            std::cerr << "need 4 traces\n";
            std::exit(2);
        }
        tr.resize(4);
        for (int c = 0; c < 4; c++)
            tr[c] = parse_trace(paths[c]);
    }

    void run()
    {
        // Per-core bookkeeping
        ready_at.assign(4, 0);
        cur_idx.assign(4, 0);
        overall_bus = Bus{};

        // Prime the first issue times by their initial gaps
        for (int c = 0; c < 4; c++)
        {
            if (cur_idx[c] < tr[c].size())
            {
                if (!tr[c][cur_idx[c]].is_mem)
                {
                    // should not happen (we coalesce compute into gap), but guard anyway
                }
            }
        }

        // Event-loop: choose next core to issue based on ready_at + next gap
        int live = 0;
        for (int c = 0; c < 4; c++)
            if (cur_idx[c] < tr[c].size())
                live++;

        while (live > 0)
        {
            int next_core = -1;
            uint64_t next_time = UINT64_MAX;
            for (int c = 0; c < 4; c++)
            {
                if (cur_idx[c] >= tr[c].size())
                    continue;
                uint64_t t = ready_at[c] + tr[c][cur_idx[c]].gap;
                if (t < next_time)
                {
                    next_time = t;
                    next_core = c;
                }
            }
            if (next_core < 0)
                break; // done

            const int c = next_core;
            auto &item = tr[c][cur_idx[c]];
            // Advance core time by compute gap
            ready_at[c] += item.gap;
            stats.advance_core_time(c, item.gap);

            if (!item.is_mem)
            {
                // should not occur; just continue
                cur_idx[c]++;
                if (cur_idx[c] == tr[c].size())
                {
                    live--;
                }
                continue;
            }

            // Processor access at time ready_at[c]
            int extra_cycles = 0, bus_bytes = 0;
            bool upgr = false;
            auto acc = caches[c].pr_access(ready_at[c], item.is_store, item.addr,
                                           extra_cycles, bus_bytes, N_words, upgr);

            if (acc.hit)
            {
                // Hit: 1 cycle service
                if (item.is_store)
                    stats.increment_stores(c);
                else
                    stats.increment_loads(c);
                stats.increment_hits(c);
                ready_at[c] += CYCLE_HIT;
            }
            else
            {
                // Miss or S->M upgrade
                if (item.is_store)
                    stats.increment_stores(c);
                else
                    stats.increment_loads(c);

                if (acc.needs_bus)
                {
                    // Decide bus op & data source via snooping
                    BusTxn t{};
                    t.op = acc.busop;
                    t.addr = item.addr;
                    t.src_core = c;
                    if (t.op == BusOp::BusUpgr)
                    {
                        // address-only
                        t.data_bytes = 0;
                        t.duration = 1;
                        // Snoop: invalidate S in others
                        // (We count one broadcast in Bus::schedule; per-recipient not needed)
                        // Simulate bus arbitration
                        uint64_t end = overall_bus.schedule(ready_at[c], t);
                        // Apply snoop to others
                        for (int k = 0; k < 4; k++)
                            if (k != c)
                                caches[k].on_busupgr(item.addr, end);
                        // Service time is 1 cycle beyond the core hit (we already add 1 hit cycle below)
                        // We model blocking: core waits for the bus upgrade before completing the store
                        stats.increment_hits(c);                           // it's a hit promotion
                        stats.increment_idle_cycles(c, end - ready_at[c]); // waiting time
                        ready_at[c] = end;                                 // now finish with the local hit
                        ready_at[c] += CYCLE_HIT;
                    }
                    else
                    {
                        // BusRd or BusRdX: check if any M owner supplies
                        bool c2c = false;
                        bool any_inval = false;
                        // First pass snoop to determine suppliers
                        for (int k = 0; k < 4; k++)
                            if (k != c)
                            {
                                if (t.op == BusOp::BusRd)
                                {
                                    auto r = caches[k].on_busrd(item.addr, ready_at[c]);
                                    if (r.supplied_block)
                                        c2c = true;
                                }
                                else if (t.op == BusOp::BusRdX)
                                {
                                    auto r = caches[k].on_busrdx(item.addr, ready_at[c]);
                                    if (r.supplied_block)
                                        c2c = true;
                                    if (r.invalidated)
                                        any_inval = true;
                                }
                            }
                        // Compute duration & bytes. We always transfer a block (either from mem or M)
                        t.data_bytes = B;
                        t.duration = c2c ? (2 * N_words) : CYCLE_MEM_BLOCK_FETCH;

                        // If we earlier added mem fetch (100) pessimistically, adjust to c2c if needed:
                        caches[c].adjust_fill_after_source(!c2c, extra_cycles, bus_bytes, N_words);

                        uint64_t end = overall_bus.schedule(ready_at[c], t);

                        // Service time at the core:
                        // - baseline 1 (like a hit)
                        // - plus extra_cycles already computed (may include WB and fetch replacement)
                        stats.increment_misses(c);
                        stats.increment_idle_cycles(c, (CYCLE_HIT + extra_cycles) - CYCLE_HIT);
                        ready_at[c] += (CYCLE_HIT + extra_cycles);
                        // Note: blocking model — we serialized WB + fetch/c2c before core proceeds
                    }
                }
                else
                {
                    // Should not occur (miss without bus), but keep safe
                    stats.increment_misses(c);
                    ready_at[c] += (CYCLE_HIT + extra_cycles);
                    stats.increment_idle_cycles(c, extra_cycles);
                }
            }

            // Advance to next op on this core
            cur_idx[c]++;
            if (cur_idx[c] == tr[c].size())
            {
                live--;
            }
        }

        // Final exec times
        for (int c = 0; c < 4; c++)
        {
            stats.set_exec_cycles(c, ready_at[c]);
        }
        stats.set_overall_bus_stats(overall_bus.total_data_bytes, overall_bus.invalidation_broadcasts);
    }

    void print_results(bool json) const
    {
        stats.print_results(json);
    }

private:
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
        uint64_t gap = 0;
        std::string la, va;
        while (in >> la >> va)
        {
            int lab = 0;
            try
            {
                lab = std::stoi(la);
            }
            catch (...)
            {
                std::cerr << "Bad label in " << path << "\n";
                std::exit(2);
            }
            if (lab == 2)
            {
                // compute gap
                uint64_t c = 0;
                if (va.size() > 2 && (va[0] == '0' && (va[1] == 'x' || va[1] == 'X')))
                    c = std::stoull(va, nullptr, 16);
                else
                    c = std::stoull(va, nullptr, 0);
                gap += c;
            }
            else
            {
                // memory access
                uint64_t addr64 = 0;
                if (va.size() > 2 && (va[0] == '0' && (va[1] == 'x' || va[1] == 'X')))
                    addr64 = std::stoull(va, nullptr, 16);
                else
                    addr64 = std::stoull(va, nullptr, 0);
                TraceItem it;
                it.gap = gap;
                it.is_mem = true;
                it.is_store = (lab == 1);
                it.addr = (uint32_t)addr64;
                out.push_back(it);
                gap = 0;
            }
        }
        // trailing compute gap is ignored (no op after it)
        return out;
    }
};
