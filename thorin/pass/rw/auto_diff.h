#ifndef THORIN_PASS_RW_AUTO_DIFF_H
#define THORIN_PASS_RW_AUTO_DIFF_H

#include "thorin/pass/pass.h"

namespace thorin {

// for multiple arguments, AD currently expects always flat elements instead of arrays
// ([mem, r64, r64, cn[...]] instead of cn[mem, <2;r64>, cn [...]]
//
//    if (ops[0]->isa<Pi>() && std::all_of(ops.begin() + 1, ops.end(), [&](auto op) { return ops[0] == op; })) return arr(n, ops[0]);
// in const Def* World::sigma(Defs ops, const Def* dbg);

// also remove
//if (std::all_of(ops.begin() + 1, ops.end(), [&](auto op) { return ops[0] == op; })) return pack(n, ops[0]);
// in const Def* World::tuple(const Def* type, Defs ops, const Def* dbg)

/*
if(auto arr = type->isa<Arr>()){
    type=arr->body();
}else {
    type=type->op(0);
}

instead of

type = type->as<Arr>()->body();
 */



/*
Automatic Differentiation based on
Backpropagation in the Simply Typed Lambda-Calculus with Linear Negation
Brunel et al, 2020
Df(x,x*) = <f(x), λa. x* (a * f'(x))>
(as x* is a pullback the call corresponds to a multiplication of the inner derivative)

This rewrite pass rewrites occurrences of the rev_diff axiom
into the differentiated versions with pullbacks.

Example:
// let sq be the squaring function x ↦ x² with the derivative 2x
// Df is a function
//   λ x. <sq(x); λ a. a * (2 * x)>
// for x* the identity pullback is created automatically
let Df = rev_diff(sq);
let yp = Df(4f); // <4²; \a -> a * (2 * 4)>
let y  = yp(0); // 16
let yP = yp(1); // \a -> a * 8
yP(1f) // 8


rewrite: Def* -> Def*
    rewrites calls of the form rev_diff(f)
        in thorin this is a call :rev_diff ‹2∷nat; r32› f
        and therefore, an app with an app as callee which has an axiom as callee
            the first argument to the outer app is a lam

reverse_diff: Lam* -> Def*
    toplevel call only used once for a rev_diff argument
    builds up initial mappings and calls j_wrap

src_to_dst:
    map from old code parts to new code
pullbacks:
    map from new code to pullback functions

j_wrap: Def* -> Def*
    builds pullback for a source code fragment
    performs main work
    corresponds to D transformation in the paper

j_wrap_rop: ROp -> Def* -> Def* -> Def*
            op     a       b
    differentiates a binary rop like addition or multiplication


in general we have
D(f(t)) =
    (x,x*) = D(t)
    <f(x); λ z. Σ xᵢ*( ∂ᵢf(x) ⋅ z )>


the transformation is mostly the identity except for functions
 a lambda f without return value is extended to receive
    a pullback for its arguments
 a returning function (having a continuation as last argument)
    changes its return type to also return a pullback
    the arguments are assumed to have an identity pullback
        (this is in agreement with the axiom)
    and the correct pullback is applied afterwards using the chain rule
    in fact, returning functions are translated using the axiom


Read-only link to overview
    https://www.overleaf.com/read/gdpfxvzqpfjf

*/


class AutoDiff : public RWPass<> {
public:
    AutoDiff(PassMan& man)
        : RWPass(man, "auto_diff")
    {}
    const Def* rewrite(const Def*) override;
};

}

#endif
