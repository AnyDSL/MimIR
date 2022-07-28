#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::affine {

/// Lowers the for axiom to actual control flow in CPS style
class LowerFor : public RWPass<Lam> {
public:
    LowerFor(PassMan& man)
        : RWPass(man, "lower_affine_for") {}

    const Def* rewrite(const Def*) override;

    static PassTag* ID();

private:
    Def2Def rewritten_;
};

} // namespace thorin::affine
