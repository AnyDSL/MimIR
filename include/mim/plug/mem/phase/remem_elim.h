#pragma once

#include <mim/phase.h>

namespace mim::plug::mem::phase {

class RememElim : public RWPhase {
public:
    RememElim(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::mem::phase
