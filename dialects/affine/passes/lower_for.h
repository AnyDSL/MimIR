#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::affine {

/// Lowers the for axiom to actual control flow in CPS.
class LowerFor : public RWPass<LowerFor, Lam> {
public:
    LowerFor(PassMan& man)
        : RWPass(man, "lower_affine_for") {}

    Ref rewrite(Ref) override;

private:
    Def2Def rewritten_;
};

} // namespace thorin::affine
