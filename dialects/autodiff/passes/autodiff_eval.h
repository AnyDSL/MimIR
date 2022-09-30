#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

class AutoDiffEval : public RWPass<AutoDiffEval, Lam> {
public:
    AutoDiffEval(PassMan& man)
        : RWPass(man, "autodiff_eval") {}

    /// detect autodiff calls
    const Def* rewrite(const Def*) override;

    /// act on toplevel autodiff on closed terms
    /// replaced lambdas, operators with the appropriate derivatives
    /// creates new lambda, calls associate variables, init maps, calls augment
    const Def* derive(const Def*);
    const Def* derive_(const Def*);

    /// applies to (open) expressions in a functional context
    /// returns the rewritten expressions and augments the partial and modular pullbacks
    /// rewrite:
    /// - id up to
    /// - renaming of variables
    /// - replacement of operations with derivatives (especially functions; operators are beta-equivalent to the
    /// original) only creates a partial pullback if one exist => not every returning expression has a pullback takes
    /// the function with respect to which the expression is differentiated
    /// can be seen as augmentation with a dual computation that generates the derivatives
    const Def* augment(const Def*, Lam*, Lam*);
    const Def* augment_(const Def*, Lam*, Lam*);
    /// helper functions for augment
    const Def* augment_var(const Var*, Lam*, Lam*);
    const Def* augment_lam(Lam*, Lam*, Lam*);
    const Def* augment_extract(const Extract*, Lam*, Lam*);
    const Def* augment_app(const App*, Lam*, Lam*);
    const Def* augment_lit(const Lit*, Lam*, Lam*);
    const Def* augment_tuple(const Tuple*, Lam*, Lam*);
    const Def* augment_pack(const Pack* pack, Lam* f, Lam* f_diff);

    /// fills partial_pullback and shadow/structure pullback maps
    void create_shadow_id_pb(const Def*);

private:
    /// expr (closed term = lambda, operator) -> derived expr
    /// f => f' = λ x. (f x, f*_x)
    /// src Def -> dst Def
    Def2Def derived;
    /// rewritten expressions (not necessarily closed) in a functional context
    /// src Def -> dst Def
    Def2Def augmented;

    /// dst Def -> dst Def
    Def2Def partial_pullback;
    /// shadows the structure of containers for additional auxiliary pullbacks
    /// a very advanced optimization might be able to recover shadow pullbacks from
    ///   the partial pullbacks
    /// example: shadow pullback of a tuple is a tuple of pullbacks
    /// not modular (composable with other pullbacks)
    /// the structure pullback only preserves structure shallowly
    ///     a n-times nested tuple has a tuple of "normal" pullbacks
    ///     each inner nested tuples should have their own structure pullback by construction
    /// e  : [B0, [B11, B12], B2]
    /// e* : [B0, [B11, B12], B2] -> A
    /// e*S: [B0 -> A, [B11, B12] -> A, B2 -> A]
    /// dst Def -> dst Def
    /// short theory of shadow pb:
    /// t: [B0, ..., Bn]
    /// t*: [B0, ..., Bn] -> A
    /// t*_S: [B0 -> A, ..., Bn -> A]
    /// b = t#i : Bi
    /// b* : Bi -> A
    /// b* = t*_S #i (if exists)
    /// equivalent to
    ///    \lambda (s:Bi). t*_S (insert s at i in (zero [B0, ..., Bn]))
    Def2Def shadow_pullback;
};

} // namespace thorin::autodiff
