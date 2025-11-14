#pragma once
#include "../coherence_protocol.hpp"
#include "../utils/stats.hpp"

class MESIProtocol : public CoherenceProtocol
{
private:
    int curr_core;
    int block_bytes;
    Bus &bus;

    enum MESIState
    {
        M,
        E,
        S,
        I,
    };

    enum MESIPrEvent
    {
        PrWr,
        PrRd,
    };

    enum MESIBusTxn
    {
        BusRdX, // BusRdX causes bus invalidations sent over the bus.
        BusRd,
    };

public:
    MESIProtocol(int curr_core, int block_bytes, Bus &bus)
        : curr_core(curr_core),
          block_bytes(block_bytes),
          bus(bus) {};

    int parse_processor_event(bool is_write, CacheLine *cache_line)
    {
        (void)cache_line; // Unused parameter.
        return is_write ? MESIPrEvent::PrWr : MESIPrEvent::PrRd;
    }

    bool on_processor_event(int processor_event, CacheLine *cache_line) override
    {
        // Set default state if invalid.
        if (!cache_line->valid)
            cache_line->state = MESIState::I;

        bool is_shared = false;
        switch (cache_line->state)
        {
        case MESIState::M:
            break;

        case MESIState::E:
            switch (processor_event)
            {
            case MESIPrEvent::PrWr:
                cache_line->state = MESIState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case MESIState::S:
            is_shared = true;
            switch (processor_event)
            {
            case MESIPrEvent::PrWr:
                // Purely invalidation requests do not contribute to bus traffic.
                is_shared = bus.trigger_bus_broadcast(curr_core, MESIBusTxn::BusRdX, cache_line, 0);
                Stats::incr_bus_invalidations();
                cache_line->state = MESIState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case MESIState::I:
            switch (processor_event)
            {
            case MESIPrEvent::PrRd:
                is_shared = bus.trigger_bus_broadcast(curr_core, MESIBusTxn::BusRd, cache_line, block_bytes / WORD_BYTES);
                cache_line->state = is_shared ? MESIState::S : MESIState::E;
                break;
            case MESIPrEvent::PrWr:
                is_shared = bus.trigger_bus_broadcast(curr_core, MESIBusTxn::BusRdX, cache_line, block_bytes / WORD_BYTES);
                Stats::incr_bus_invalidations();
                cache_line->state = MESIState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        default:
            std::cerr << "Invalid MESI state\n";
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
        case MESIState::M:
            cache_line->dirty = false;
            switch (bus_transaction)
            {
            case MESIBusTxn::BusRd:
                // Data flushed via main memory writeback, since dirty bit is lost.
                bus.access_main_memory(curr_core, CYCLE_WRITEBACK_DIRTY);
                cache_line->dirty = false;
                cache_line->state = MESIState::S;
                break;
            case MESIBusTxn::BusRdX:
                // Data flushed via cache-to-cache transfer.
                cache_line->valid = false;
                cache_line->state = MESIState::I;
                break;
            }
            break;

        case MESIState::E:
            switch (bus_transaction)
            {
            case MESIBusTxn::BusRd:
                cache_line->state = MESIState::S;
                break;
            case MESIBusTxn::BusRdX:
                cache_line->valid = false;
                cache_line->state = MESIState::I;
                break;
            }
            break;

        case MESIState::S:
            switch (bus_transaction)
            {
            case MESIBusTxn::BusRd:
                cache_line->state = MESIState::S;
                break;
            case MESIBusTxn::BusRdX:
                cache_line->valid = false;
                cache_line->state = MESIState::I;
                break;
            }
            break;

        case MESIState::I:
            break;

        default:
            std::cerr << "Invalid MESI state\n";
            break;
        }

        return;
    }
};
