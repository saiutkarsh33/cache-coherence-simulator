#ifndef CACHE_CPP
#define CACHE_CPP

#include <vector>
#include <iostream>
#include "types.hpp"
#include "constants.hpp"

// For Part 1 (single-core), bus address-only timing has no effect on concurrency, so we do not model it.
// We only count DATA bytes on the bus per the spec (no address bytes).

struct CacheLine
{
    u32 tag = 0;
    bool valid = false;
    bool dirty = false;
    // Track a minimal MESI-like classification for reporting only (single-core never sees S):
    // 'I', 'E', 'M'
    char state = 'I';
    u64 lru_stamp = 0;
};

struct CacheSet
{
    std::vector<CacheLine> lines;
    explicit CacheSet(int assoc) : lines(assoc) {}
};

class L1Cache
{
public:
    L1Cache(int size_bytes, int assoc, int block_bytes)
        : size_bytes_(size_bytes), assoc_(assoc), block_bytes_(block_bytes)
    {

        if (assoc_ <= 0 || block_bytes_ <= 0 || size_bytes_ <= 0)
        {
            std::cerr << "Cache parameters must be positive integers.\n";
            std::exit(2);
        }
        if ((size_bytes_ % (assoc_ * block_bytes_)) != 0)
        {
            std::cerr << "Cache size must be a multiple of (associativity * block_size).\n";
            std::exit(2);
        }
        num_sets_ = size_bytes_ / (assoc_ * block_bytes_);
        sets_.reserve(num_sets_);
        for (int i = 0; i < num_sets_; ++i)
            sets_.emplace_back(assoc_);

        // init stats
        hits_ = misses_ = 0;
        bus_bytes_ = 0;
        invalid_or_updates_ = 0; // 0 in single-core
        private_accesses_ = shared_accesses_ = 0;
        writebacks_ = 0;
        access_clock_ = 0;
    }

    struct AccessResult
    {
        bool hit;
        int extra_cycles;   // beyond the 1-cycle hit
        int bus_data_bytes; // bytes added to data bus traffic for this access
    };

    AccessResult access(int op /*0=load,1=store*/, u32 addr)
    {
        ++access_clock_;

        auto [index, tag] = index_tag(addr);
        auto [line_ptr, way] = find_line(index, tag);

        if (line_ptr != nullptr)
        {
            // -------- Hit --------
            ++hits_;
            line_ptr->lru_stamp = access_clock_;
            // classify access
            if (line_ptr->state == 'E' || line_ptr->state == 'M')
                ++private_accesses_;
            else
                ++shared_accesses_; // should not occur in single-core

            if (op == 1)
            {
                if (line_ptr->state == 'E')
                    line_ptr->state = 'M';
                line_ptr->dirty = true;
            }
            return {true, 0, 0}; // caller adds 1 cycle for hit
        }

        // -------- Miss --------
        ++misses_;
        auto [victim, vway] = choose_victim(index);

        int extra_cycles = 0;
        int bus_bytes = 0;

        // Eviction if needed
        if (victim->valid && victim->dirty)
        {
            extra_cycles += CYCLE_WRITEBACK_DIRTY;
            bus_bytes += block_bytes_;
            bus_bytes_ += block_bytes_;
            ++writebacks_;
        }

        // Fetch block from memory
        extra_cycles += CYCLE_MEM_BLOCK_FETCH;
        bus_bytes += block_bytes_;
        bus_bytes_ += block_bytes_;

        // Fill
        victim->valid = true;
        victim->tag = tag;
        victim->lru_stamp = access_clock_;
        if (op == 0)
        {
            victim->state = 'E';
            victim->dirty = false;
            ++private_accesses_;
        }
        else
        {
            victim->state = 'M';
            victim->dirty = true;
            ++private_accesses_;
        }

        return {false, extra_cycles, bus_bytes};
    }

    // Expose stats
    u64 hits() const { return hits_; }
    u64 misses() const { return misses_; }
    u64 bus_bytes() const { return bus_bytes_; }
    u64 invalid_or_updates() const { return invalid_or_updates_; }
    u64 private_accesses() const { return private_accesses_; }
    u64 shared_accesses() const { return shared_accesses_; }
    u64 writebacks() const { return writebacks_; }

private:
    std::pair<int, int> index_tag(u32 addr) const
    {
        u32 line_addr = addr / block_bytes_;
        int index = static_cast<int>(line_addr % num_sets_);
        int tag = static_cast<int>(line_addr / num_sets_);
        return {index, tag};
    }

    std::pair<CacheLine *, int> find_line(int index, int tag)
    {
        auto &lines = sets_[index].lines;
        for (int w = 0; w < assoc_; ++w)
        {
            auto &line = lines[w];
            if (line.valid && static_cast<int>(line.tag) == tag)
                return {&line, w};
        }
        return {nullptr, -1};
    }

    std::pair<CacheLine *, int> choose_victim(int index)
    {
        auto &lines = sets_[index].lines;
        for (int w = 0; w < assoc_; ++w)
        {
            if (!lines[w].valid)
                return {&lines[w], w};
        }
        // LRU: smallest lru_stamp
        int victim_way = 0;
        u64 best = lines[0].lru_stamp;
        for (int w = 1; w < assoc_; ++w)
        {
            if (lines[w].lru_stamp < best)
            {
                best = lines[w].lru_stamp;
                victim_way = w;
            }
        }
        return {&lines[victim_way], victim_way};
    }

    int size_bytes_;
    int assoc_;
    int block_bytes_;
    int num_sets_;

    std::vector<CacheSet> sets_;
    u64 access_clock_;

    // stats
    u64 hits_;
    u64 misses_;
    u64 bus_bytes_;
    u64 invalid_or_updates_;
    u64 private_accesses_;
    u64 shared_accesses_;
    u64 writebacks_;
};

#endif
