#pragma once
#include <cstdint>
#include "bus.hpp"
#include "utils/types.hpp"

// Abstract base class for coherence protocol implementations.
// Coherence protocol implemented like a state machine.
//
// Handle processor events: coherence state transitions, snoop handling.
class CoherenceProtocol
{
private:
public:
    virtual ~CoherenceProtocol() = default;

    virtual int parse_processor_event(bool is_write, CacheLine *cache_line) = 0;

    // Handle processor-initiated access for the cache line.
    // Returns true if the cache line is shared, otherwise false.
    //
    // Abstract such that on_processor_event should not handle the valid bit,
    // but should handle the dirty bit.
    virtual bool on_processor_event(int processor_event, CacheLine *cache_line) = 0;

    // Handle snoop events (bus transactions) from the bus,
    // which originate from another core.
    //
    // Assume that snoop hits on a modified line are served via cache-to-cache transfer,
    // rather than immediately writing back to main memory.
    //
    // Snooping transactions are non blocking, happen instantanesouly,
    // and do not advance exec time for the core.
    // As such, we do not update LRU time for snoop operations (core-centric LRU).
    virtual void on_snoop_event(int bus_transaction, CacheLine *cache_line) = 0;
};
