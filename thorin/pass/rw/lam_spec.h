#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

class LamSpec : public RWPass<Lam> {
public:
    LamSpec(PassMan& man)
        : RWPass(man, "lam_spec") {}

    static PassTag* ID();

private:
    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    ///@}

    Def2Def old2new_;
};

} // namespace thorin
