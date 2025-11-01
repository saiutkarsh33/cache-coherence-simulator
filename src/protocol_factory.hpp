#pragma once
#include <memory>
#include <string>
#include "coherence_protocol.hpp"
#include "mesi/mesi_protocol.hpp"
#include "dragon/dragon_protocol.hpp"
#include "cache.hpp"

std::unique_ptr<CoherenceProtocol> make_protocol(const std::string &name, int curr_core, int block_size, Bus &bus)
{
    if (name == "MESI")
    {
        return std::make_unique<MESIProtocol>(curr_core, block_size, bus);
    }
    else if (name == "Dragon")
    {
        return std::make_unique<DragonProtocol>(curr_core, block_size, bus);
    }
    else
    {
        throw std::runtime_error("Unknown protocol: " + name);
    }
}

std::unique_ptr<Cache> make_cache(const std::string &protocol_name,
                                  int cache_size, int assoc, int block_size, int curr_core, Bus &bus)
{
    auto protocol = make_protocol(protocol_name, curr_core, block_size, bus);
    return std::make_unique<Cache>(cache_size, assoc, block_size, protocol.release(), curr_core);
}
