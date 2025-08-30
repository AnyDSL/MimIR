#pragma once

#include "mim/phase/phase.h"

namespace mim {

class PrefixCleanup : public RWPhase {
public:
    PrefixCleanup(World& world, std::string prefix = "internal_")
        : RWPhase(world, "prefix_cleanup " + prefix)
        , prefix_(std::move(prefix)) {}

    void start() final;

private:
    std::string prefix_;
};

} // namespace mim
