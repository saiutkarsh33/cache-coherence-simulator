#pragma once
#include "../coherence_protocol.hpp"
#include "../utils/stats.hpp"
#include <ostream>

class DragonProtocol : public CoherenceProtocol
{
private:
    int curr_core;
    int block_bytes;
    Bus &bus;

    enum DragonState
    {
        E,
        Sc,
        Sm,
        M,
    };

    enum DragonPrEvent
    {
        PrRd,
        PrRdMiss,
        PrWr,
        PrWrMiss,
    };

    enum DragonBusTxn
    {
        BusRd,
        BusUpd, // BusUpd causes bus updates sent over the bus.
    };

public:
    DragonProtocol(int curr_core, int block_bytes, Bus &bus)
        : curr_core(curr_core),
          block_bytes(block_bytes),
          bus(bus) {};

    int parse_processor_event(bool is_write, CacheLine *cache_line) override
    {
        if (is_write)
        {
            return (cache_line == nullptr || !cache_line->valid) ? DragonPrEvent::PrWrMiss : DragonPrEvent::PrWr;
        }
        else
        {
            return (cache_line == nullptr || !cache_line->valid) ? DragonPrEvent::PrRdMiss : DragonPrEvent::PrRd;
        }
    }

    bool on_processor_event(int processor_event, CacheLine *cache_line) override
    {
        // Handle all cache miss process events here (this sets the default state).
        // Trigger bus rd broadcast to other cores if cache miss, which should transfer N words from one core to another as cache missed.
        bool is_shared = false;
        switch (processor_event)
        {
        case DragonPrEvent::PrRdMiss:
            is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusRd, cache_line, block_bytes / WORD_BYTES);
            cache_line->state = is_shared ? DragonState::Sc : DragonState::E;
            break;

        case DragonPrEvent::PrWrMiss:
            is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusRd, cache_line, block_bytes / WORD_BYTES);
            cache_line->state = is_shared ? DragonState::Sm : DragonState::M;
            cache_line->dirty = true;

            // Processor writes misses also trigger a bus update (sends a word from one cache to another).
            is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusUpd, cache_line, 1);
            Stats::incr_bus_updates();
            break;
        }

        // Handle cache hit processor events here:
        // Skip PrRd entirely as these do not affect the state.
        switch (cache_line->state)
        {
        case DragonState::E:
            switch (processor_event)
            {
            case DragonPrEvent::PrWr:
                cache_line->state = DragonState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case DragonState::Sc:
            is_shared = true;
            switch (processor_event)
            {
            case DragonPrEvent::PrWr:
                // BusUpd sends a word from one cache to another.
                is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusUpd, cache_line, 1);
                Stats::incr_bus_updates();
                cache_line->state = is_shared ? DragonState::Sm : DragonState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case DragonState::Sm:
            is_shared = true;
            switch (processor_event)
            {
            case DragonPrEvent::PrWr:
                // BusUpd sends a word from one cache to another.
                is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusUpd, cache_line, 1);
                Stats::incr_bus_updates();
                cache_line->state = is_shared ? DragonState::Sm : DragonState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case DragonState::M:
            break;

        default:
            std::cerr << "Invalid Dragon state\n";
            break;
        }

        return is_shared;
    }

    void on_snoop_event(int bus_transaction, CacheLine *cache_line) override
    {
        // If invalid, no snoop processing required.
        if (cache_line == nullptr || !cache_line->valid)
            return;

        switch (cache_line->state)
        {
        case DragonState::E:
            switch (bus_transaction)
            {
            case DragonBusTxn::BusRd:
                cache_line->state = DragonState::Sc;
                break;
            }
            break;

        case DragonState::Sc:
            break;

        case DragonState::Sm:
            switch (bus_transaction)
            {
            case DragonBusTxn::BusUpd:
                cache_line->state = DragonState::Sc;
                cache_line->dirty = false; // Data flushed via cache-to-cache transfer.
                break;
            }
            break;

        case DragonState::M:
            switch (bus_transaction)
            {
            case DragonBusTxn::BusRd:
                cache_line->state = DragonState::Sm;
                cache_line->dirty = false; // Data flushed via cache-to-cache transfer.
                break;
            }
            break;

        default:
            std::cerr << "Invalid Dragon state\n";
            break;
        }

        return;
    }
};