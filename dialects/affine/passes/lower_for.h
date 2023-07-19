#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::affine {

/// Lowers the for axiom to actual control flow in CPS.
/// ```
/// .con body(iter: %core.I8, (acc0: .Nat, acc1: .Bool), yield: .Cn [.Nat, .Bool]) = yield (new_acc0, new_acc1);
/// .con exit(out0: .Nat, out1: .Bool) = use(out0, out1);
/// %affine.For (256, 2, (.Nat, .Bool)) (begin, end, step, (0, .ff), body, exit)
/// ```
/// =>
/// ```
/// .con head(iter: %core.I8, acc0: .Nat, acc1: .Bool) =
///     .let add = %core.wrap.add %core.nusw (iter, step);
///     .let cmp = %core.icmp.ul (iter, end);
///     .con new_body() = head (add, new_acc0, new_acc1);
///     .con new_exit() = use (new_acc0, new_acc1);
///     (new_exit, new_body)#cmp ()
/// head (begin, 0, .ff)
/// ```
/// * We have to merge `init`/`acc` into the signature, as it may contain a `mem`.
/// * In this case we have to equip `new_body`/`new_exit` with a `mem` as well.
class LowerFor : public RWPass<LowerFor, Lam> {
public:
    LowerFor(PassMan& man)
        : RWPass(man, "lower_affine_for") {}

    Ref rewrite(Ref) override;

private:
    Def2Def rewritten_;
};

} // namespace thorin::affine
