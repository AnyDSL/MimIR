#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::plug::direct {

/// Converts direct style function to cps functions.
/// To do so, for each (non-type-level) ds function a corresponding cps function is created:
/// ```
/// f:     Π a: A -> B
/// f_cps: .Cn [a: A, .Cn B]
/// ```
/// Only the type signature of the function is changed and the body is wrapped in the newly added return continuation.
/// (Technical detail: the arguments are substituted to fit the new function)
///
/// In a second distinct but connected step, the call sites are converted:
/// For a direct style call `f args`, the call to the cps function `cps2ds_dep ... f_cps args` is introduced.
/// The underlying substitution is `f` -> `cps2ds_dep ... f_cps`.
class DS2CPS : public RWPass<DS2CPS, Lam> {
public:
    DS2CPS(PassMan& man)
        : RWPass(man, "ds2cps") {}

    Ref rewrite(Ref) override;

private:
    Def2Def rewritten_;

    Ref rewrite_lam(Lam* lam);
};

} // namespace thorin::plug::direct
