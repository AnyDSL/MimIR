#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

const Def* create_ho_pb_type(const Def* def, const Def* arg_ty);

/// This pass is the heart of AD.
/// We replace an `autodiff fun` call with the differentiated function.
class AutoDiffEval : public RWPass<AutoDiffEval, Lam> {
public:
    AutoDiffEval(PassMan& man)
        : RWPass(man, "autodiff_eval") {}

    /// Detect autodiff calls.
    const Def* rewrite(const Def*) override;

    /// Acts on toplevel autodiff on closed terms:
    /// * Replaces lambdas, operators with the appropriate derivatives.
    /// * Creates new lambda, calls associate variables, init maps, calls augment.
    const Def* derive(const Def*);
    const Def* derive_(const Def*);

    /// Applies to (open) expressions in a functional context.
    /// Returns the rewritten expressions and augments the partial and modular pullbacks.
    /// The rewrite is identity on the term up to renaming of variables.
    /// Otherwise, only pullbacks are added.
    /// To do so, some calls (e.g. axioms) are replaced by their derivatives.
    /// This transformation can be seen as an augmentation with a dual computation that generates the derivatives.
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
    const Def* augment_lea(const App*, Lam*, Lam*);
    const Def* augment_load(const App*, Lam*, Lam*);
    const Def* augment_store(const App*, Lam*, Lam*);
    const Def* augment_malloc(const App*, Lam*, Lam*);
    const Def* augment_alloc(const App*, Lam*, Lam*);
    const Def* augment_bitcast(const App*, Lam*, Lam*);

    const Def* autodiff_zero(const Def* mem, Lam* f);
    const Def* autodiff_zero(const Def* mem, const Def* def);

    const Def* zero_pullback(const Def* domain, Lam* f);


    const Def* autodiff_epilogue(Lam* f_outer, Lam* f_inner, const Def* diff_ty);
    const Def* wrap_call_pullbacks(const Def* arg_pb, const Def* arg);
    Lam* create_gradient_collector(const Def* gradient_array, Lam* f);
    const Def* get_pullback(const Def* op, Lam* f);
    Lam* wrap_free_memory(const Def* pullback);

private:
    /// Transforms closed terms (lambda, operator) to derived expressions.
    /// `f => f' = λ x. (f x, f*_x)`
    /// src Def -> dst Def
    Def2Def derived;
    /// Associates expressions (not necessarily closed) in a functional context to their derivatived counterpart.
    /// src Def -> dst Def
    Def2Def augmented;

    /// dst Def -> dst Def
    Def2Def partial_pullback;

    /// Shadows the structure of containers for additional auxiliary pullbacks.
    /// A very advanced optimization might be able to recover shadow pullbacks from the partial pullbacks.
    /// Example: The shadow pullback of a tuple is a tuple of pullbacks.
    /// Shadow pullbacks are not modular (composable with other pullbacks).
    /// The structure pullback only preserves structure shallowly:
    /// A n-times nested tuple has a tuple of "normal" pullbacks as shadow pullback.
    /// Each inner nested tuples should have their own structure pullback by construction.
    /// ```
    /// e  : [B0, [B11, B12], B2]
    /// e* : [B0, [B11, B12], B2] -> A
    /// e*S: [B0 -> A, [B11, B12] -> A, B2 -> A]
    /// ```
    /// short theory of shadow pb:
    /// ```
    /// t: [B0, ..., Bn]
    /// t*: [B0, ..., Bn] -> A
    /// t*_S: [B0 -> A, ..., Bn -> A]
    /// b = t#i : Bi
    /// b* : Bi -> A
    /// b* = t*_S #i (if exists)
    /// ```
    /// This is equivalent to:
    /// `\lambda (s:Bi). t*_S (insert s at i in (zero [B0, ..., Bn]))`
    /// dst Def -> dst Def
    Def2Def shadow_pullback;

    // TODO: remove?
    Def2Def app_pb;

    Def2Def shadow_pullbacks;
    Def2Def gradient_ptrs;

    DefSet allocated_memory;
    DefSet caches;
};

} // namespace thorin::autodiff
