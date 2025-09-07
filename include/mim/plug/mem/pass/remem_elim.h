#pragma once

#include <mim/pass.h>

namespace mim::plug::mem {

class RememElim : public RWPass<RememElim, Lam> {
public:
    RememElim(World& world, flags_t annex)
        : RWPass(world, annex) {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::mem
