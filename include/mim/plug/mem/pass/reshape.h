#pragma once

#include <queue>

#include "mim/phase.h"

namespace mim::plug::mem {

using DefQueue = std::deque<const Def*>;

/// The general idea of this pass/phase is to change the shape of signatures of functions.
/// * Example: `Cn[ [mem,  A, B], C  , ret]`
/// * Arg    : `Cn[ [mem, [A, B , C]], ret]` (general `Cn[ [mem, args], ret]`)
/// * Flat   : `Cn[  mem,  A, B , C  , ret]` (general `Cn[mem, ...args, ret]`)
/// For convenience, we want Arg-style for optimizations.
/// The invariant is that every closed function has at most one "real" argument and a return-continuation.
/// If memory is present, the argument is a pair of memory and the remaining arguments.
/// However, flat style is required for code generation. Especially in the closure conversion.
///
/// The concept is to rewrite all signatures of functions with consistent reassociation of arguments.
/// This change is propagated to (nested) applications.
// TODO: use RWPhase instead
class Reshape : public RWPass<Reshape, Lam> {
public:
    enum Mode { Flat, Arg };

    Reshape(PassMan& man, Mode mode)
        : RWPass(man, "reshape")
        , mode_(mode) {}

    /// Fall-through to `rewrite_def` which falls through to `rewrite_lam`.
    void enter() override;

private:
    /// Memoized version of `rewrite_def_`
    const Def* rewrite_def(const Def* def);
    /// Replace lambas with reshaped versions, shape application arguments, and replace vars and already rewritten
    /// lambdas.
    const Def* rewrite_def_(const Def* def);
    /// Create a new lambda with the reshaped signature and rewrite its body.
    /// The old var is associated with a reshaped version of the new var in `old2new_`.
    Lam* reshape_lam(Lam* def);

    /// Reshapes a type into its flat or arg representation.
    const Def* reshape_type(const Def* T);
    /// Reshapes a def into its flat or arg representation.
    const Def* reshape(const Def* def);
    // This generalized version of reshape transforms def to match the shape of target.
    const Def* reshape(const Def* def, const Def* target);
    /// Reconstructs the target type by taking defs out of the queue.
    const Def* reshape(DefVec&, const Def* target, const Def* mem);

    /// Keeps track of the replacements.
    Def2Def old2new_;
    /// The mode to rewrite all lambas to. Either flat or arg.
    Mode mode_;
};

} // namespace mim::plug::mem
