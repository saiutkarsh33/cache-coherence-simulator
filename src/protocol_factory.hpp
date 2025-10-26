// protocol_factory.hpp
#pragma once
#include <memory>
#include <string>
#include <stdexcept>
#include "coherence_protocol.hpp"
#include "mesi/mesi_protocol.hpp"
#include "dragon/dragon_protocol.hpp"
#include "hybrid/hybrid_protocol.hpp"

inline std::unique_ptr<CoherenceProtocol> make_protocol(const std::string &name,
                                                        int cache_size, int assoc, int block_size)
{
    if (name == "MESI" || name == "mesi")
    {
        return std::make_unique<MESIProtocol>(cache_size, assoc, block_size);
    }
    else if (name == "Dragon" || name == "dragon")
    {
        return std::make_unique<DragonProtocol>(cache_size, assoc, block_size);
    }
    else if (name == "Hybrid" || name == "hybrid")
    {
        auto h = std::make_unique<HybridProtocol>(cache_size, assoc, block_size);
        // optionally configure threshold here
        h->set_threshold(2);
        return h;
    }

    throw std::runtime_error("Unknown protocol: " + name);
}
