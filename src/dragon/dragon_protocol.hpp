#pragma once
#include "../coherence_protocol.hpp"
#include "../utils/stats.hpp"

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
        NoOp,
        BusRd,
        BusUpd,
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
        // Handle all cache miss process events here.
        // Trigger bus rd broadcast to other cores if cache miss.
        bool is_shared = false;
        switch (processor_event)
        {
        case DragonPrEvent::PrRdMiss:
            // We handle adding idle cycles and bus traffic bytes outside this func (since cache miss).
            is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusRd, cache_line->addr);
            cache_line->state = is_shared ? DragonState::Sc : DragonState::E;
            break;

        case DragonPrEvent::PrWrMiss:
            // We handle adding idle cycles and bus traffic bytes outside this func (since cache miss).
            is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusRd, cache_line->addr);
            cache_line->state = is_shared ? DragonState::Sm : DragonState::M;
            cache_line->dirty = true;
            break;
        }

        // Handle cache hit processor events here:
        switch (cache_line->state)
        {
        case DragonState::E:
            switch (processor_event)
            {
            case DragonPrEvent::PrWr:
                cache_line->state = DragonState::M;
                break;
            }
            break;

        case DragonState::Sc:
            switch (processor_event)
            {
            case DragonPrEvent::PrWr:
                is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusUpd, cache_line->addr);
                Stats::incr_bus_updates();
                Stats::add_idle_cycles(curr_core, block_bytes * 2); // Sending a cache block with takes 2N.
                Stats::add_bus_traffic_bytes(block_bytes);          // Assume that traffic is still sent even though it may be not shared.
                cache_line->state = is_shared ? DragonState::Sm : DragonState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case DragonState::Sm:
            switch (processor_event)
            {
            case DragonPrEvent::PrWr:
                is_shared = bus.trigger_bus_broadcast(curr_core, DragonBusTxn::BusUpd, cache_line->addr);
                Stats::incr_bus_updates();
                Stats::add_idle_cycles(curr_core, block_bytes * 2); // Sending a cache block with takes 2N.
                Stats::add_bus_traffic_bytes(block_bytes);          // Assume that traffic is still sent even though it may be not shared.
                cache_line->state = is_shared ? DragonState::Sm : DragonState::M;
                cache_line->dirty = true;
                break;
            }
            break;

        case DragonState::M:
            break;

        default:
            std::runtime_error("invalid Dragon state");
            break;
        }

        return is_shared;
    }

    void on_snoop_event(int bus_transaction, CacheLine *cache_line) override
    {
        // If invalid, no snoop processing required.
        if (!cache_line->valid)
            return;

        switch (bus_transaction)
        {
        case DragonBusTxn::BusRd:
            switch (cache_line->state)
            {
            case DragonState::E:
                cache_line->state = DragonState::Sc;
                break;
            case DragonState::M:
                // Flush that core (core to core transfer).
                Stats::add_bus_traffic_bytes(block_bytes); // FIXME?
                cache_line->state = DragonState::Sm;
                break;
            }
            break;

        case DragonBusTxn::BusUpd:
            switch (cache_line->state)
            {
            case DragonState::Sm:
                cache_line->state = DragonState::Sc;
                break;
            }
            break;

        case DragonBusTxn::NoOp:
            break;

        default:
            std::runtime_error("invalid Dragon bus transaction");
            break;
        }

        return;
    }
};