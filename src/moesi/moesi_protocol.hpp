#pragma once
#include "../coherence_protocol.hpp"
#include "../utils/stats.hpp"

// MOESI Protocol: Optimization of MESI with "Owned" state
// 
// Key Optimization: The Owned (O) state allows a cache to share dirty data
// with other caches WITHOUT writing back to memory first.
//
// Benefits:
// 1. Reduced memory traffic (no writeback on M->O transitions)
// 2. Lower latency for sharing modified data
// 3. Better cache-to-cache transfer efficiency
//
// Literature:
// - Sweazey & Smith (1986): "A class of compatible cache consistency protocols"
// - AMD Opteron, ARM Cortex-A series use variants of MOESI
//
// States:
// - M (Modified): Exclusive, dirty, must respond to snoops
// - O (Owned): Shared, dirty, responsible for supplying data (optimization!)
// - E (Exclusive): Exclusive, clean
// - S (Shared): Shared, clean
// - I (Invalid): Invalid

class MOESIProtocol : public CoherenceProtocol
{
private:
    int curr_core;
    int block_bytes;
    Bus &bus;

    enum MOESIState
    {
        M,  // Modified (exclusive, dirty)
        O,  // Owned (shared, dirty) - THE OPTIMIZATION!
        E,  // Exclusive (exclusive, clean)
        S,  // Shared (shared, clean)
        I,  // Invalid
    };

    enum MOESIPrEvent
    {
        PrWr,
        PrRd,
    };

    enum MOESIBusTxn
    {
        BusRdX, // Exclusive read (invalidates others)
        BusRd,  // Shared read
    };

public:
    MOESIProtocol(int curr_core, int block_bytes, Bus &bus)
        : curr_core(curr_core),
          block_bytes(block_bytes),
          bus(bus) {};

    int parse_processor_event(bool is_write, CacheLine *cache_line) override
    {
        (void)cache_line; // Unused parameter
        return is_write ? MOESIPrEvent::PrWr : MOESIPrEvent::PrRd;
    }

    bool on_processor_event(int processor_event, CacheLine *cache_line) override
    {
        // Set default state if invalid
        if (!cache_line->valid)
            cache_line->state = MOESIState::I;

        bool is_shared = false;
        
        switch (cache_line->state)
        {
        case MOESIState::M:
            // Modified: already have exclusive dirty copy
            break;

        case MOESIState::O:
            // Owned: have dirty shared copy
            is_shared = true;
            switch (processor_event)
            {
            case MOESIPrEvent::PrWr:
                // Need to invalidate other sharers
                bus.trigger_bus_broadcast(curr_core, MOESIBusTxn::BusRdX, cache_line, block_bytes / WORD_BYTES);
                Stats::incr_bus_invalidations();
                cache_line->state = MOESIState::M;
                cache_line->dirty = true;
                is_shared = false; // now exclusive
                break;
            }
            break;

        case MOESIState::E:
            // Exclusive clean
            switch (processor_event)
            {
            case MOESIPrEvent::PrWr:
                // Silent upgrade E->M (no bus transaction needed!)
                cache_line->state = MOESIState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case MOESIState::S:
            // Shared clean
            is_shared = true;
            switch (processor_event)
            {
            case MOESIPrEvent::PrWr:
                // Need to invalidate other sharers
                bus.trigger_bus_broadcast(curr_core, MOESIBusTxn::BusRdX, cache_line, block_bytes / WORD_BYTES);
                Stats::incr_bus_invalidations();
                cache_line->state = MOESIState::M;
                cache_line->dirty = true;
                is_shared = false; // now exclusive
                break;
            }
            break;

        case MOESIState::I:
            // Invalid - need to fetch
            switch (processor_event)
            {
            case MOESIPrEvent::PrRd:
                is_shared = bus.trigger_bus_broadcast(curr_core, MOESIBusTxn::BusRd, cache_line, block_bytes / WORD_BYTES);
                cache_line->state = is_shared ? MOESIState::S : MOESIState::E;
                break;
            case MOESIPrEvent::PrWr:
                is_shared = bus.trigger_bus_broadcast(curr_core, MOESIBusTxn::BusRdX, cache_line, block_bytes / WORD_BYTES);
                Stats::incr_bus_invalidations();
                cache_line->state = MOESIState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        default:
            std::cerr << "Invalid MOESI state\n";
            break;
        }

        return is_shared;
    }

    void on_snoop_event(int bus_transaction, CacheLine *cache_line) override
    {
        // If invalid, no snoop processing required
        if (cache_line == nullptr || !cache_line->valid)
            return;

        switch (cache_line->state)
        {
        case MOESIState::M:
            // OPTIMIZATION: M->O transition on BusRd (no writeback!)
            // Supply data via cache-to-cache transfer
            Stats::add_bus_traffic_bytes(block_bytes);
            switch (bus_transaction)
            {
            case MOESIBusTxn::BusRd:
                // KEY OPTIMIZATION: Transition to Owned instead of writing back!
                // Data remains dirty but can be shared
                cache_line->state = MOESIState::O;
                // dirty flag stays true - still responsible for eventual writeback
                break;
            case MOESIBusTxn::BusRdX:
                // Another core wants exclusive access - invalidate
                cache_line->valid = false;
                cache_line->dirty = false; // data transferred
                cache_line->state = MOESIState::I;
                break;
            }
            break;

        case MOESIState::O:
            // Owned: responsible for supplying data to sharers
            Stats::add_bus_traffic_bytes(block_bytes);
            switch (bus_transaction)
            {
            case MOESIBusTxn::BusRd:
                // Stay in Owned, supply data
                cache_line->state = MOESIState::O;
                break;
            case MOESIBusTxn::BusRdX:
                // Another core wants exclusive access - invalidate
                cache_line->valid = false;
                cache_line->dirty = false; // data transferred
                cache_line->state = MOESIState::I;
                break;
            }
            break;

        case MOESIState::E:
            // Exclusive clean
            switch (bus_transaction)
            {
            case MOESIBusTxn::BusRd:
                // Downgrade to Shared
                cache_line->state = MOESIState::S;
                break;
            case MOESIBusTxn::BusRdX:
                // Invalidate
                cache_line->valid = false;
                cache_line->state = MOESIState::I;
                break;
            }
            break;

        case MOESIState::S:
            // Shared clean
            switch (bus_transaction)
            {
            case MOESIBusTxn::BusRd:
                // Stay in Shared
                cache_line->state = MOESIState::S;
                break;
            case MOESIBusTxn::BusRdX:
                // Invalidate
                cache_line->valid = false;
                cache_line->state = MOESIState::I;
                break;
            }
            break;

        case MOESIState::I:
            // Already invalid
            break;

        default:
            std::cerr << "Invalid MOESI state\n";
            break;
        }

        return;
    }
};