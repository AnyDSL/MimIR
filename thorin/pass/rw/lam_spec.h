#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

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

} // namespace thorin
