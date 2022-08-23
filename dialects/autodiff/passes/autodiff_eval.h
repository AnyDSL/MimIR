#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

class AutoDiffEval : public RWPass<AutoDiffEval,Lam> {
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
    /// - replacement of operations with derivatives (especially functions; operators are beta-equivalent to the original)
    const Def* augment(const Def*);
    const Def* augment_(const Def*);

    static PassTag* ID();

    const Def* mod_pb(const Def* def);

private:
    /// expr (closed term = lambda, operator) -> derived expr 
    /// f => f' = λ x. (f x, f*_x)
    /// src Def -> dst Def
    /// for continuations the partial derivative (TODO: maybe split? - or move to partial pullback?)
    Def2Def derived;
    /// rewritten expressions (not necessarily closed) in a functional context
    /// src Def -> dst Def
    Def2Def augmented;

    // remove: a modular pullback does not exist on its own
    // Def2Def modular_pullbacks;

    /// dst Def -> dst Def
    Def2Def partial_pullbacks;

    Def2Def app_pb;
};

} // namespace thorin::autodiff
