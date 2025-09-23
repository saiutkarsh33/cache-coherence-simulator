// coherence.cpp — CS4223 A2 Part 1 (single-core) in C++17
// Implements: blocking L1 D-cache (WB/WA, LRU), DRAM latency, bus DATA traffic accounting.
// CLI matches assignment: coherence <protocol> <input_file> <cache_size> <associativity> <block_size> [--json]
// Protocol arg is accepted for CLI parity; Part 1 logic is protocol-agnostic by design.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <sys/stat.h>

using u64 = unsigned long long;
using u32 = uint32_t;

// -------------------
// Tunable constants (assumptions stated in the report)
// -------------------
static constexpr int CYCLE_HIT = 1;                // L1 hit latency (cycles) — given
static constexpr int CYCLE_MEM_BLOCK_FETCH = 100;  // fetch a block from memory — given
static constexpr int CYCLE_WRITEBACK_DIRTY = 100;  // dirty writeback on eviction — given            // 32-bit word — given

// For Part 1 (single-core), bus address-only timing has no effect on concurrency, so we do not model it.
// We only count DATA bytes on the bus per the spec (no address bytes).

// -------------------
// Utility
// -------------------
static inline bool file_exists(const std::string& path) {
    struct stat sb {};
    return ::stat(path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode);
}

static u64 parse_auto_base(const std::string& s) {
    // Accepts "1234", "0x4d2", etc.
    std::size_t idx = 0;
    u64 v = 0;
    try {
        v = std::stoull(s, &idx, 0); // base 0 -> auto-detect (0x for hex)
    } catch (...) {
        std::cerr << "Failed to parse numeric value: '" << s << "'\n";
        std::exit(2);
    }
    if (idx != s.size()) {
        std::cerr << "Trailing characters in numeric value: '" << s << "'\n";
        std::exit(2);
    }
    return v;
}

static std::string resolve_part1_trace_path(const std::string& input) {
    if (file_exists(input)) return input;
    std::string alt = input + "_0.data";
    if (file_exists(alt)) return alt;
    std::cerr << "Could not find trace file: '" << input << "' or '" << alt << "'\n";
    std::exit(2);
}

// -------------------
// Cache structures
// -------------------
struct CacheLine {
    u32 tag = 0;
    bool valid = false;
    bool dirty = false;
    // Track a minimal MESI-like classification for reporting only (single-core never sees S):
    // 'I', 'E', 'M'
    char state = 'I';
    u64 lru_stamp = 0;
};

struct CacheSet {
    std::vector<CacheLine> lines;
    explicit CacheSet(int assoc) : lines(assoc) {}
};

class L1Cache {
public:
    L1Cache(int size_bytes, int assoc, int block_bytes)
        : size_bytes_(size_bytes), assoc_(assoc), block_bytes_(block_bytes) {

        if (assoc_ <= 0 || block_bytes_ <= 0 || size_bytes_ <= 0) {
            std::cerr << "Cache parameters must be positive integers.\n";
            std::exit(2);
        }
        if ((size_bytes_ % (assoc_ * block_bytes_)) != 0) {
            std::cerr << "Cache size must be a multiple of (associativity * block_size).\n";
            std::exit(2);
        }
        num_sets_ = size_bytes_ / (assoc_ * block_bytes_);
        sets_.reserve(num_sets_);
        for (int i = 0; i < num_sets_; ++i) sets_.emplace_back(assoc_);

        // init stats
        hits_ = misses_ = 0;
        bus_bytes_ = 0;
        invalid_or_updates_ = 0; // 0 in single-core
        private_accesses_ = shared_accesses_ = 0;
        writebacks_ = 0;
        access_clock_ = 0;
    }

    struct AccessResult {
        bool hit;
        int extra_cycles;     // beyond the 1-cycle hit
        int bus_data_bytes;   // bytes added to data bus traffic for this access
    };

    AccessResult access(int op /*0=load,1=store*/, u32 addr) {
        ++access_clock_;

        auto [index, tag] = index_tag(addr);
        auto [line_ptr, way] = find_line(index, tag);

        if (line_ptr != nullptr) {
            // -------- Hit --------
            ++hits_;
            line_ptr->lru_stamp = access_clock_;
            // classify access
            if (line_ptr->state == 'E' || line_ptr->state == 'M') ++private_accesses_;
            else ++shared_accesses_; // should not occur in single-core

            if (op == 1) {
                if (line_ptr->state == 'E') line_ptr->state = 'M';
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
        if (victim->valid && victim->dirty) {
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
        if (op == 0) {
            victim->state = 'E';
            victim->dirty = false;
            ++private_accesses_;
        } else {
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
    std::pair<int,int> index_tag(u32 addr) const {
        u32 line_addr = addr / block_bytes_;
        int index = static_cast<int>(line_addr % num_sets_);
        int tag = static_cast<int>(line_addr / num_sets_);
        return {index, tag};
    }

    std::pair<CacheLine*, int> find_line(int index, int tag) {
        auto& lines = sets_[index].lines;
        for (int w = 0; w < assoc_; ++w) {
            auto& line = lines[w];
            if (line.valid && static_cast<int>(line.tag) == tag) return {&line, w};
        }
        return {nullptr, -1};
    }

    std::pair<CacheLine*, int> choose_victim(int index) {
        auto& lines = sets_[index].lines;
        for (int w = 0; w < assoc_; ++w) {
            if (!lines[w].valid) return {&lines[w], w};
        }
        // LRU: smallest lru_stamp
        int victim_way = 0;
        u64 best = lines[0].lru_stamp;
        for (int w = 1; w < assoc_; ++w) {
            if (lines[w].lru_stamp < best) {
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

struct CoreStats {
    u64 exec_cycles = 0;    // time at end
    u64 compute_cycles = 0; // sum of "2 value" entries
    u64 idle_cycles = 0;    // cycles waiting beyond 1-cycle hit
    u64 loads = 0;
    u64 stores = 0;
    u64 hits = 0;
    u64 misses = 0;
    u64 private_accesses = 0;
    u64 shared_accesses = 0;
};

static void simulate_single_core(const std::string& trace_path,
                                 int cache_size, int assoc, int block_size,
                                 bool json_output)
{
    L1Cache cache(cache_size, assoc, block_size);
    CoreStats stats;
    u64 t = 0; // cycle time

    std::ifstream in(trace_path);
    if (!in) {
        std::cerr << "Failed to open trace file: " << trace_path << "\n";
        std::exit(2);
    }

    std::string lab_s, val_s;
    while (in >> lab_s >> val_s) {
        int lab = 0;
        try { lab = std::stoi(lab_s); }
        catch (...) {
            std::cerr << "Bad label in trace: '" << lab_s << "'\n";
            std::exit(2);
        }
        u64 val = parse_auto_base(val_s);

        if (lab == 2) {
            // compute (other instructions): advance time
            stats.compute_cycles += val;
            t += val;
            continue;
        }

        // memory op
        int op = lab; // 0=load, 1=store
        if (op == 0) ++stats.loads; else ++stats.stores;

        auto res = cache.access(op, static_cast<u32>(val)); // addresses are 32-bit per spec

        // service time: 1 on hit; 1 + extra on miss
        int service = CYCLE_HIT + res.extra_cycles;
        if (service > CYCLE_HIT) stats.idle_cycles += (service - CYCLE_HIT);
        t += service;
    }

    stats.exec_cycles = t;
    stats.hits = cache.hits();
    stats.misses = cache.misses();
    stats.private_accesses = cache.private_accesses();
    stats.shared_accesses = cache.shared_accesses();
    const u64 bus_data_bytes = cache.bus_bytes();
    const u64 inv_or_updates = cache.invalid_or_updates();

    if (json_output) {
        // Minimal JSON without external deps.
        std::cout << std::fixed;
        std::cout << "{\n";
        std::cout << "  \"overall_execution_cycles\": " << stats.exec_cycles << ",\n";
        std::cout << "  \"per_core_execution_cycles\": [" << stats.exec_cycles << "],\n";
        std::cout << "  \"per_core_compute_cycles\": [" << stats.compute_cycles << "],\n";
        std::cout << "  \"per_core_loads\": [" << stats.loads << "],\n";
        std::cout << "  \"per_core_stores\": [" << stats.stores << "],\n";
        std::cout << "  \"per_core_idle_cycles\": [" << stats.idle_cycles << "],\n";
        std::cout << "  \"per_core_hits\": [" << stats.hits << "],\n";
        std::cout << "  \"per_core_misses\": [" << stats.misses << "],\n";
        std::cout << "  \"bus_data_traffic_bytes\": " << bus_data_bytes << ",\n";
        std::cout << "  \"bus_invalidations_or_updates\": " << inv_or_updates << ",\n";
        std::cout << "  \"private_accesses\": [" << stats.private_accesses << "],\n";
        std::cout << "  \"shared_accesses\": [" << stats.shared_accesses << "],\n";
        std::cout << "  \"config\": {\"cache_size\": " << cache_size
                  << ", \"associativity\": " << assoc
                  << ", \"block_size\": " << block_size << "}\n";
        std::cout << "}\n";
    } else {
        std::cout << "Overall Execution Cycles: " << stats.exec_cycles << "\n";
        std::cout << "Per-core execution cycles: [" << stats.exec_cycles << "]\n";
        std::cout << "Compute cycles per core:  [" << stats.compute_cycles << "]\n";
        std::cout << "Loads/stores per core:    " << stats.loads << " / " << stats.stores << "\n";
        std::cout << "Idle cycles per core:     [" << stats.idle_cycles << "]\n";
        std::cout << "Hits/misses per core:     " << stats.hits << " / " << stats.misses << "\n";
        std::cout << "Bus data traffic (bytes): " << bus_data_bytes << "\n";
        std::cout << "Invalidations/Updates:    " << inv_or_updates << "\n";
        std::cout << "Private vs Shared:        " << stats.private_accesses
                  << " / " << stats.shared_accesses << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0]
                  << " <protocol: MESI|Dragon> <input_file> <cache_size> <associativity> <block_size> [--json]\n";
        return 2;
    }

    std::string protocol = argv[1]; // accepted but not used in Part 1
    if (protocol != "MESI" && protocol != "Dragon") {
        std::cerr << "Protocol must be MESI or Dragon.\n";
        return 2;
    }
    std::string input = argv[2];
    int cache_size = std::stoi(argv[3]);
    int assoc      = std::stoi(argv[4]);
    int block_size = std::stoi(argv[5]);

    bool json_output = false;
    for (int i = 6; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--json") json_output = true;
    }

    std::string trace_path = resolve_part1_trace_path(input);
    simulate_single_core(trace_path, cache_size, assoc, block_size, json_output);
    return 0;
}
