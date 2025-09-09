#pragma once

#include <mim/pass.h>

namespace mim::plug::mem {

class RememElim : public RWPass<RememElim, Lam> {
public:
    RememElim(World& world, flags_t annex)
        : RWPass(world, annex) {}
    std::unique_ptr<Stage> recreate() final { return std::make_unique<RememElim>(world(), annex()); }

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::mem
