#pragma once

#include <mim/pass.h>

namespace mim::plug::clos {

class BranchClosElim : public RWPass<BranchClosElim, Lam> {
public:
    BranchClosElim(World& world, flags_t annex)
        : RWPass(world, annex) {}
    BranchClosElim* recreate() final { return driver().stage<BranchClosElim>(world(), annex()); }

    const Def* rewrite(const Def*) override;

private:
    DefMap<Lam*> branch2dropped_;
};

}; // namespace mim::plug::clos
