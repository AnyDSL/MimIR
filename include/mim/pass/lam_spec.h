#pragma once

#include "mim/pass.h"

namespace mim {

class LamSpec : public RWPass<LamSpec, Lam> {
public:
    LamSpec(World& world, flags_t annex)
        : RWPass(world, annex) {}
    std::unique_ptr<Stage> recreate() final { return std::make_unique<LamSpec>(world(), annex()); }

private:
    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    ///@}

    Def2Def old2new_;
};

} // namespace mim
