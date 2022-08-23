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
    const Def* augment(const Def*, Lam*, Lam*);
    const Def* augment_(const Def*, Lam*, Lam*);

    /// fills partial_pullback and shadow/structure pullback maps
    void create_shadow_id_pb(const Def*);

    static PassTag* ID();

    const Def* mod_pb(const Def* def);

private:
    /// expr (closed term = lambda, operator) -> derived expr
    /// f => f' = λ x. (f x, f*_x)
    /// src Def -> dst Def
    /// R: for continuations the partial derivative (TODO: maybe split? - or move to partial pullback?)
    ///  ^ not needed (handled by var augmentation)
    Def2Def derived;
    /// rewritten expressions (not necessarily closed) in a functional context
    /// src Def -> dst Def
    Def2Def augmented;

    // remove: a modular pullback does not exist on its own
    // Def2Def modular_pullbacks;

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
    /// dst Def -> dst Def
    Def2Def shadow_pullback;

    Def2Def app_pb;
};

} // namespace thorin::autodiff
