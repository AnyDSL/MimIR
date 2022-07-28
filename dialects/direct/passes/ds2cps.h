#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::direct {

/// This pass converts a direct style functions (abbreviated as ds) into CPS.
/// the main conversion is shown in this pseudocode:
/// ```
/// h:
///   b = f a
///   C[b]
/// ```
/// becomes
/// ```
/// h:
///     f'(a,h_cont)
///
/// h_cont(b):
///     C[b]
/// ```
/// with the following types:
/// ```
/// f : A -> B
/// f': .Cn [A, ret: .Cn[B]]
/// ```
/// The idea is to create a cps function for each ds function
/// invoke the cps function with a continuation that takes the
/// computation result and uses it in the original context.
/// For each ds function, a new cps function is introduced.
/// For each ds call site, a new continuation is introduced.
class DS2CPS : public RWPass<Lam> {
public:
    DS2CPS(PassMan& man)
        : RWPass(man, "ds2cps") {}

    void enter() override;

    static PassTag* ID();

private:
    Def2Def rewritten_;
    Def2Def rewritten_bodies_;
    Lam* curr_lam_ = nullptr;

    void rewrite_lam(Lam* lam);
    const Def* rewrite_(const Def*);
    const Def* rewrite_inner(const Def*);
};

} // namespace thorin::direct
