#pragma once

#include <mim/phase.h>

namespace mim::plug::eqsat {

class EqsatPhase : public Phase {
public:
    EqsatPhase(World& world, std::string name)
        : Phase(world, std::move(name)) {}
    EqsatPhase(World& world, flags_t annex)
        : Phase(world, annex) {}

    void start() override;
};
}; // namespace mim::plug::eqsat
