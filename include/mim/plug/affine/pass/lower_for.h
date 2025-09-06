#pragma once

#include <mim/def.h>
#include <mim/pass.h>

namespace mim::plug::affine {

/// Lowers the for axm to actual control flow in CPS.
/// It basically mimics this implementation:
/// ```
/// con %affine.For_impl
///     (m: Nat , n: Nat , Ts: «n; *»)
///     (begin: Idx m, end: Idx m, step: Idx m, init: «i: n; Ts#i»,
///             body: Cn [iter: Idx m, acc: «i: n; Ts#i», yield: Cn «i: n; Ts#i»],
///             exit: Cn «i: n; Ts#i»
///     ) =
///     con head(iter: Idx m, acc: «i: n; Ts#i») =
///         con new_body() = body (iter, acc, cn acc: «i: n; Ts#i» =
///             let iter2 = %core.wrap.add %core.mode.nsuw (iter, step);
///             head (iter2, acc));
///         con new_exit() = exit (acc);
///         (new_exit, new_body)#(%core.icmp.ul (iter, end)) ();
///     head(begin, init);
/// ```
/// However, we merge `init`/`acc` into the signature, as it may contain a `mem`.
/// In this case we have to equip `new_body`/`new_exit` with a `mem` as well.
/// @todo We probably want to have phases which fix such things so we don't have to do this in C++.
class LowerFor : public RWPass<LowerFor, Lam> {
public:
    LowerFor(PassMan& man)
        : RWPass(man, "lower_affine_for") {}

    const Def* rewrite(const Def*) override;

private:
    Def2Def rewritten_;
};

} // namespace mim::plug::affine
