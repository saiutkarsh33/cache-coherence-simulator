// coherence_sim.hpp — generalized 4-core simulator
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
#include <memory>
#include "bus.hpp"
#include "coherence_protocol.hpp"
#include "protocol_factory.hpp"
#include "utils/trace_item.hpp"
#include "utils/stats.hpp"
#include "utils/types.hpp"

class CacheSim
{
private:
    int block_bytes;
    int words_per_block;

    Bus overall_bus;
    std::vector<std::unique_ptr<CoherenceProtocol>> caches; // polymorphic caches

    Stats stats;
    std::vector<std::vector<TraceItem>> traces;
    std::vector<u64> ready_at;
    std::vector<size_t> cur_idx;

    int cache_size, assoc;

public:
    CacheSim(const std::string &protocol_name, int cache_size_, int assoc_, int block_size)
        : block_bytes(block_size),
          words_per_block(block_size / WORD_BYTES),
          stats(cache_size_, assoc_, block_size),
          cache_size(cache_size_), assoc(assoc_)
    {
        assert(block_bytes > 0 && (block_bytes % WORD_BYTES) == 0);
        // create four protocol instances (one per core)
        caches.reserve(4);
        for (int i = 0; i < 4; ++i)
        {
            caches.push_back(make_protocol(protocol_name, cache_size_, assoc_, block_size));
        }
    }

    void load_traces(const std::vector<std::string> &paths)
    {
        if (paths.size() != 4)
        {
            std::cerr << "need 4 traces\n";
            std::exit(2);
        }
        traces.resize(4);
        for (int c = 0; c < 4; c++)
            traces[c] = parse_trace(paths[c]);
    }

    void run()
    {
        ready_at.assign(4, 0);
        cur_idx.assign(4, 0);
        overall_bus = Bus{};

        while (true)
        {
            int next_core = -1;
            u64 next_time = UINT64_MAX;
            for (int c = 0; c < 4; c++)
            {
                int max_idx = (int)traces[c].size();
                while (cur_idx[c] < max_idx && traces[c][cur_idx[c]].op == Operation::Other)
                {
                    auto trace_item = traces[c][cur_idx[c]];
                    ready_at[c] += trace_item.cycles;
                    stats.advance_core_time(c, trace_item.cycles);
                    cur_idx[c]++;
                }
                if (cur_idx[c] < max_idx && ready_at[c] < next_time)
                {
                    next_time = ready_at[c];
                    next_core = c;
                }
            }
            if (next_core < 0)
                break;
            const int curr_core = next_core;
            TraceItem trace_item = traces[curr_core][cur_idx[curr_core]];
            stats.increment_mem_op(curr_core, trace_item.op);

            int extra_cycles = 0, bus_bytes = 0;
            bool upgr = false;

            auto acc = caches[curr_core]->pr_access(ready_at[curr_core],
                                                    trace_item.op == Operation::Store,
                                                    trace_item.addr,
                                                    extra_cycles, bus_bytes,
                                                    words_per_block, upgr);

            if (acc.hit)
            {
                stats.increment_hits(curr_core);
                ready_at[curr_core] += CYCLE_HIT;
            }
            else
            {
                if (acc.needs_bus)
                {
                    BusTxn t{};
                    t.op = acc.busop;
                    t.addr = trace_item.addr;
                    t.src_core = curr_core;
                    if (t.op == BusOp::BusUpgr)
                    {
                        t.data_bytes = 0;
                        t.duration = 1;
                        u64 end = overall_bus.schedule(ready_at[curr_core], t);
                        // apply snoop
                        for (int k = 0; k < 4; k++)
                            if (k != curr_core)
                            {
                                auto r = caches[k]->on_busupgr(trace_item.addr, end);
                                (void)r;
                            }
                        stats.increment_hits(curr_core);
                        stats.increment_idle_cycles(curr_core, end - ready_at[curr_core]);
                        ready_at[curr_core] = end;
                        ready_at[curr_core] += CYCLE_HIT;
                    }
                    else
                    {
                        bool c2c = false;
                        bool any_inval = false;
                        for (int k = 0; k < 4; k++)
                            if (k != curr_core)
                            {
                                if (t.op == BusOp::BusRd)
                                {
                                    auto r = caches[k]->on_busrd(trace_item.addr, ready_at[curr_core]);
                                    if (r.supplied_block)
                                        c2c = true;
                                }
                                else if (t.op == BusOp::BusRdX)
                                {
                                    auto r = caches[k]->on_busrdx(trace_item.addr, ready_at[curr_core]);
                                    if (r.supplied_block)
                                        c2c = true;
                                    if (r.invalidated)
                                        any_inval = true;
                                }
                                else if (t.op == BusOp::BusUpd)
                                {
                                    // update broadcast: other caches should update their copies
                                    // We model update by calling on_busrd (or a dedicated handler if desired)
                                    auto r = caches[k]->on_busrd(trace_item.addr, ready_at[curr_core]);
                                    // r.supplied_block not expected here; updates don't supply
                                }
                            }
                        t.data_bytes = acc.is_write ? block_bytes : block_bytes; // block transfer
                        t.duration = c2c ? (2 * words_per_block) : CYCLE_MEM_BLOCK_FETCH;
                        // let per-protocol object adjust extra cycles if needed
                        // Some protocol implementations provide adjust_fill_after_source; we'll call via dynamic cast if available
                        // For simplicity call schedule:
                        uint64_t end = overall_bus.schedule(ready_at[curr_core], t);

                        stats.increment_misses(curr_core);
                        stats.increment_idle_cycles(curr_core, (CYCLE_HIT + extra_cycles) - CYCLE_HIT);
                        ready_at[curr_core] += (CYCLE_HIT + extra_cycles);
                    }
                }
                else
                {
                    stats.increment_misses(curr_core);
                    ready_at[curr_core] += (CYCLE_HIT + extra_cycles);
                    stats.increment_idle_cycles(curr_core, extra_cycles);
                }
            }

            cur_idx[curr_core]++;
        }

        for (int c = 0; c < 4; c++)
            stats.set_exec_cycles(c, ready_at[c]);
        stats.set_overall_bus_stats(overall_bus.total_data_bytes, overall_bus.invalidation_broadcasts);
    }

    void print_results(bool json) const { stats.print_results(json); }
};
