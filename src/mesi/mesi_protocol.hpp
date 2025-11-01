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
        NoOp,
        BusRdX,
        BusRd,
    };

public:
    MESIProtocol(int curr_core, int block_bytes, Bus &bus)
        : curr_core(curr_core),
          block_bytes(block_bytes),
          bus(bus) {};

    int parse_processor_event(bool is_write, CacheLine *cache_line)
    {
        return is_write ? MESIPrEvent::PrWr : MESIPrEvent::PrRd;
    }

    bool on_processor_event(int processor_event, CacheLine *cache_line) override
    {
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
            switch (processor_event)
            {
            case MESIPrEvent::PrWr:
                is_shared = bus.trigger_bus_broadcast(curr_core, MESIBusTxn::BusRdX, cache_line->addr);
                cache_line->state = MESIState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case MESIState::I:
            switch (processor_event)
            {
            case MESIPrEvent::PrRd:
                // We handle adding idle cycles and bus traffic bytes outside this func (since cache miss).
                is_shared = bus.trigger_bus_broadcast(curr_core, MESIBusTxn::BusRd, cache_line->addr);
                cache_line->state = is_shared ? MESIState::S : MESIState::E;
                break;
            case MESIPrEvent::PrWr:
                // We handle adding idle cycles and bus traffic bytes outside this func (since cache miss).
                is_shared = bus.trigger_bus_broadcast(curr_core, MESIBusTxn::BusRdX, cache_line->addr);
                cache_line->state = MESIState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        default:
            std::runtime_error("invalid MESI state");
            break;
        }

        return is_shared;
    }

    void on_snoop_event(int bus_transaction, CacheLine *cache_line) override
    {
        if (!cache_line->valid)
            return;

        switch (cache_line->state)
        {
        case MESIState::M:
            // Data flushed via cache-to-cache transfer.
            Stats::add_bus_traffic_bytes(block_bytes);
            cache_line->dirty = false;
            switch (bus_transaction)
            {
            case MESIBusTxn::BusRd:
                cache_line->state = MESIState::S;
                break;
            case MESIBusTxn::BusRdX:
                Stats::incr_bus_invalidations();
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
                Stats::incr_bus_invalidations();
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
                Stats::incr_bus_invalidations();
                cache_line->valid = false;
                cache_line->state = MESIState::I;
                break;
            }
            break;

        case MESIState::I:
            break;

        default:
            std::runtime_error("invalid MESI state");
            break;
        }

        return;
    }
};
