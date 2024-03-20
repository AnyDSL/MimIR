#pragma once

#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

/// Recurseivly rewrites part of a program **into** the provided World.
/// This World may be different than the World we started with.
class Rewriter {
public:
    Rewriter(World& world)
        : world_(world) {}

    World& world() { return world_; }
    /// Map @p old_def to @p new_def and returns @p new_def;
    Ref map(Ref old_def, Ref new_def) { return old2new_[old_def] = new_def; }

    /// @name rewrite
    ///@{
    /// Recursively rewrite old Def%s.
    virtual Ref rewrite(Ref);
    virtual Ref rewrite_imm(Ref);
    virtual Ref rewrite_mut(Def*);
    ///@}

private:
    World& world_;
    Def2Def old2new_;
};

class VarRewriter : public Rewriter {
public:
    VarRewriter(Ref var, Ref arg)
        : Rewriter(var->world()) {
        if (var) {
            if (auto v = var->isa<Var>()) {
                map(var, arg);
                vars_ = world().vars(v);
            }
        }
    }

    Ref rewrite_imm(Ref imm) override {
        if (imm->local_vars().empty() && imm->local_muts().empty()) return imm; // safe to skip
        return Rewriter::rewrite_imm(imm);
    }

    Ref rewrite_mut(Def* mut) override {
        if (world().has_intersection(mut->free_vars(), vars_)) {
            if (auto var = mut->has_var()) vars_ = world().insert(vars_, var);
            return Rewriter::rewrite_mut(mut);
        }
        return map(mut, mut);
    }

private:
    Vars vars_;
};

/// @name rewrite
///@{
/// Rewrites @p mut's @p i^th op by substituting @p mut's @p Var with @p arg while obeying @p mut's @p scope.
Ref rewrite(Def* mut, Ref arg, size_t i);

/// Rewrites @p mut's ops by substituting @p mut's @p Var with @p arg while obeying @p mut's @p scope.
DefVec rewrite(Def* mut, Ref arg);
///@}

} // namespace thorin
