#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::direct {

/// Direct style function convertion to cps functions
/// creates for each (non-type-level) ds function
/// a corresponding cps function
/// f: Π a : A -> B
/// f_cps : cn[a:A, cn B]
/// only the type signature is changed
/// and the body is wrapped in the return continuation
/// (technical detail: the arguments are substituted to fit the new function)
///
/// In a second distinct but connected step, the call sites are converted
/// for a direct style call `f args`, the
/// call to the cps function `cps2ds_dep ... f_cps args` is introduced
/// the substitution is `f` -> `cps2ds_dep ... f_cps`
///
class DS2CPS : public RWPass<DS2CPS, Lam> {
public:
    DS2CPS(PassMan& man)
        : RWPass(man, "ds2cps") {}

    void enter() override;
    // const Def* rewrite(const Def*) override;

    Def2Def get_subst() { return rewritten_; }

private:
    Def2Def rewritten_;

    void rewrite_lam(Lam* lam);
    // const Def* rewrite_inner(const Def*);
};

} // namespace thorin::direct
