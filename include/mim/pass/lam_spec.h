#pragma once

#include "mim/pass.h"

namespace mim {

class LamSpec : public RWPass<LamSpec, Lam> {
public:
    LamSpec(PassMan& man)
        : RWPass(man, "lam_spec") {}

private:
    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    ///@}

    Def2Def old2new_;
};

} // namespace mim
