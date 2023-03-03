#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::affine {

/// Lowers the for axiom to actual control flow in CPS style
class LowerFor : public RWPass<LowerFor, Lam> {
public:
    LowerFor(PassMan& man)
        : RWPass(man, "lower_affine_for")
        , sym_{
            .acc_   = world().sym("acc"),
            .begin_ = world().sym("begin"),
            .body_  = world().sym("body"),
            .break_ = world().sym("break"),
            .end_   = world().sym("end"),
            .for_   = world().sym("for"),
            .step_  = world().sym("step"),
            .yield_ = world().sym("yield")} {}

    const Def* rewrite(const Def*) override;

private:
    struct { Sym acc_, begin_, body_, break_, end_, for_, step_, yield_; } sym_;
    Def2Def rewritten_;
};

} // namespace thorin::affine
