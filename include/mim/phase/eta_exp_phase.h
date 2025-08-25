#pragma once

#include "mim/phase/phase.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class EtaExpPhase : public FPPhase {
public:
    EtaExpPhase(World& world)
        : FPPhase(world, "beta reduction") {}

    /// @name Lattice
    /// EtaExpPhase uses the following lattice:
    /// ```
    ///       expand_            <-- η-expand non-callee as the Lam occurs more than once.
    ///        /   \                 Don't η-reduce the expansion again.
    ///  Callee     Non_Callee_1 <-- Pos: Either multiple callees, **or** exactly one non-callee are okay.
    ///        \   /
    ///         Bot              <-- Never seen.
    /// ```
    /// * EtaExp::expand_ trackes if a Lam is the top element.
    /// * If it's not within EtaExp::expand_, it's either `Pos` or `Bot`.
    ///   But there is no need to differentiate this any further.
    /// * EtaExp::Pos tracks the particular element at that level and is memorized statefully.
    ///@{
    enum Pos { Callee, Non_Callee_1 };

    bool analyze() final;

private:
    void analyze(const Def*);
    const Def* rewrite_imm(const Def*) final;
    const Def* rewrite_mut(Def*) final;

    DefSet analyzed_;
    LamMap<bool> candidate_;
};

} // namespace mim
