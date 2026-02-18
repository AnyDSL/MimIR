#pragma once

#include <mim/phase.h>

namespace mim::plug::eqsat {

class EggRewrite : public RWPhase {
public:
    EggRewrite(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    void start() override;
};

}; // namespace mim::plug::eqsat
