#pragma once

#include "mim/pass/pass.h"

namespace mim {

class LamSpec : public RWPass<LamSpec, Lam> {
public:
    LamSpec(PassMan& man)
        : RWPass(man, "lam_spec") {}

private:
    /// @name PassMan hooks
    ///@{
    Ref rewrite(Ref) override;
    ///@}

    Def2Def old2new_;
};

} // namespace mim
