#pragma once

#include <mim/pass.h>

namespace mim::plug::mem {

class Alloc2Malloc : public RWPass<Alloc2Malloc, Lam> {
public:
    Alloc2Malloc(World& world, flags_t annex)
        : RWPass(world, annex) {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::mem
