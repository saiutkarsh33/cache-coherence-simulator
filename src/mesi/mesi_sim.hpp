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
#include "../bus.hpp"
#include "mesi_protocol.hpp"
#include "../utils/trace_item.hpp"
#include "../utils/stats.hpp"
#include "../utils/types.hpp"

class MESISim
{
private:
    int block_bytes;     // block bytes
    int words_per_block; // words per block

    Bus overall_bus;
    MESIProtocol caches[NUM_OF_CORES];

    Stats stats;

    // traces stores the input trace files as TraceItems per core.
    std::vector<std::vector<TraceItem>> traces;
    // ready_at tracks the time which a core is ready to take in the next insn per core.
    std::vector<u64> ready_at;
    // cur_idx tracks the current insn called per core.
    std::vector<size_t> cur_idx;

public:
    MESISim(int cache_size, int assoc, int block_size)
        : block_bytes(block_size),
          words_per_block(block_size / WORD_BYTES),
          caches{MESIProtocol(cache_size, assoc, block_size),
                 MESIProtocol(cache_size, assoc, block_size),
                 MESIProtocol(cache_size, assoc, block_size),
                 MESIProtocol(cache_size, assoc, block_size)},
          stats(cache_size, assoc, block_size, "MESI")
    {
        assert(block_bytes > 0 && (block_bytes % WORD_BYTES) == 0);
    }

    void load_traces(const std::vector<std::string> &paths)
    {
        if (paths.size() != NUM_OF_CORES)
        {
            std::cerr << "need " << NUM_OF_CORES << " traces\n";
            std::exit(2);
        }

        traces.resize(4);
        for (int c = 0; c < NUM_OF_CORES; c++)
        {
            traces[c] = parse_trace(paths[c]);
        }
    }

    // run handles the event loop across multiple cores.
    void run()
    {
        // Per-core bookkeeping
        ready_at.assign(NUM_OF_CORES, 0);
        cur_idx.assign(NUM_OF_CORES, 0);
        overall_bus = Bus{};

        // Event-loop: choose next core to issue based on which is ready first.
        while (true)
        {
            // Determine which core to use.
            int next_core = -1;
            u64 next_time = UINT64_MAX;
            for (int c = 0; c < NUM_OF_CORES; c++)
            {
                int max_idx = traces[c].size();

                // Iterate until we process all compute cycles for a core.
                while (cur_idx[c] < max_idx && traces[c][cur_idx[c]].op == Operation::Other)
                {
                    TraceItem trace_item = traces[c][cur_idx[c]];

                    // Advance ready at and core time by compute cycles.
                    ready_at[c] += trace_item.cycles;
                    stats.advance_core_time(c, trace_item.cycles);

                    // Increment the idx.
                    cur_idx[c]++;
                }

                // Check if the core has the smallest ready at time out of all cores.
                if (cur_idx[c] < max_idx && ready_at[c] < next_time)
                {
                    next_time = ready_at[c];
                    next_core = c;
                }
            }
            if (next_core < 0)
            {
                // No other core to process, terminate event loop.
                break;
            }

            // The next_core represents the core which is ready the earliest
            const int curr_core = next_core;

            // trace_item must be either a Load or a Store here.
            TraceItem trace_item = traces[curr_core][cur_idx[curr_core]];
            stats.increment_mem_op(curr_core, trace_item.op);

            // Processor access at time ready_at[c]
            int extra_cycles = 0, bus_bytes = 0;
            bool upgr = false;
            auto acc = caches[curr_core].pr_access(
                ready_at[curr_core],
                trace_item.op == Operation::Store,
                trace_item.addr,
                extra_cycles, bus_bytes, words_per_block, upgr);

            if (acc.hit)
            {
                // Hit: 1 cycle service
                stats.increment_hits(curr_core);
                ready_at[curr_core] += CYCLE_HIT;

                // Classify access by MESI state
                MESIState state = caches[curr_core].get_line_state(trace_item.addr);
                if (state == MESIState::M || state == MESIState::E)
                    stats.increment_private_access(curr_core);
                else if (state == MESIState::S)
                    stats.increment_shared_access(curr_core);
            }
            else
            {
                // Miss or S->M upgrade
                if (acc.needs_bus)
                {
                    // Decide bus op & data source via snooping
                    BusTxn t{};
                    t.op = acc.busop;
                    t.addr = trace_item.addr;
                    t.src_core = curr_core;
                    if (t.op == BusOp::BusUpgr)
                    {
                        // address-only
                        t.data_bytes = 0;
                        t.duration = 1;
                        // Snoop: invalidate S in others
                        // (We count one broadcast in Bus::schedule; per-recipient not needed)
                        // Simulate bus arbitration
                        u64 end = overall_bus.schedule(ready_at[curr_core], t);
                        // Apply snoop to others
                        for (int k = 0; k < NUM_OF_CORES; k++)
                            if (k != curr_core)
                                caches[k].on_busupgr(trace_item.addr, end);

                        // Service time is 1 cycle beyond the core hit (we already add 1 hit cycle below)
                        // We model blocking: core waits for the bus upgrade before completing the store
                        stats.increment_hits(curr_core);                                   // it's a hit promotion
                        stats.increment_idle_cycles(curr_core, end - ready_at[curr_core]); // waiting time
                        ready_at[curr_core] = end;                                         // now finish with the local hit
                        ready_at[curr_core] += CYCLE_HIT;
                    }
                    else
                    {
                        // BusRd or BusRdX: check if any M owner supplies.
                        bool c2c = false;
                        bool any_inval = false;

                        // First pass snoop to determine suppliers.
                        for (int k = 0; k < NUM_OF_CORES; k++)
                            if (k != curr_core)
                            {
                                if (t.op == BusOp::BusRd)
                                {
                                    auto r = caches[k].on_busrd(trace_item.addr, ready_at[curr_core]);
                                    if (r.supplied_block)
                                        c2c = true;
                                }
                                else if (t.op == BusOp::BusRdX)
                                {
                                    auto r = caches[k].on_busrdx(trace_item.addr, ready_at[curr_core]);
                                    if (r.supplied_block)
                                        c2c = true;
                                    if (r.invalidated)
                                        any_inval = true;
                                }
                            }
                        // Compute duration & bytes. We always transfer a block (either from mem or M).
                        t.data_bytes = block_bytes;
                        t.duration = c2c ? (2 * words_per_block) : CYCLE_MEM_BLOCK_FETCH;

                        // If we earlier added mem fetch (100) pessimistically, adjust to c2c if needed:
                        caches[curr_core].adjust_fill_after_source(!c2c, extra_cycles, bus_bytes, words_per_block);

                        uint64_t end = overall_bus.schedule(ready_at[curr_core], t);

                        // Service time at the core:
                        // - baseline 1 (like a hit)
                        // - plus extra_cycles already computed (may include WB and fetch replacement)
                        stats.increment_misses(curr_core);
                        stats.increment_idle_cycles(curr_core, (CYCLE_HIT + extra_cycles) - CYCLE_HIT);
                        ready_at[curr_core] += (CYCLE_HIT + extra_cycles);
                        // Note: blocking model — we serialized WB + fetch/c2c before core proceeds.
                    }
                }
                else
                {
                    // Should not occur (miss without bus), but keep safe.
                    stats.increment_misses(curr_core);
                    ready_at[curr_core] += (CYCLE_HIT + extra_cycles);
                    stats.increment_idle_cycles(curr_core, extra_cycles);
                }
            }

            // Advance to next operation on this core.
            cur_idx[curr_core]++;
        }

        // Update stats with final exec times.
        for (int c = 0; c < NUM_OF_CORES; c++)
        {
            stats.set_exec_cycles(c, ready_at[c]);
        }
        stats.set_overall_bus_stats(overall_bus.total_data_bytes, overall_bus.invalidation_or_update_broadcasts);
    }

    void print_results(bool json) const
    {
        stats.print_results(json);
    }
};
