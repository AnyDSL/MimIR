#pragma once

#include "mim/def.h"
#include "mim/world.h"

namespace mim {

/// Recurseivly rewrites part of a program **into** the provided World.
/// This World may be different than the World we started with.
class Rewriter {
public:
    Rewriter(World& world)
        : world_(world) {}

    World& world() { return world_; }
    /// Map @p old_def to @p new_def and returns @p new_def;
    const Def* map(const Def* old_def, const Def* new_def) { return old2new_[old_def] = new_def; }

    /// @name rewrite
    /// Recursively rewrite old Def%s.
    ///@{
    virtual const Def* rewrite(const Def*);
    virtual const Def* rewrite_imm(const Def*);
    virtual const Def* rewrite_mut(Def*);
    ///@}

private:
    World& world_;
    Def2Def old2new_;
};

class VarRewriter : public Rewriter {
public:
    VarRewriter(const Var* var, const Def* arg)
        : Rewriter(arg->world())
        , vars_(var ? Vars({var}) : Vars()) {
        if (var) map(var, arg);
    }

    const Def* rewrite_imm(const Def* imm) override {
        if (imm->local_vars().empty() && imm->local_muts().empty()) return imm; // safe to skip
        if (imm->has_dep(Dep::Hole) || has_intersection(vars_, imm->free_vars())) return Rewriter::rewrite_imm(imm);
        return imm;
    }

    const Def* rewrite_mut(Def* mut) override {
        if (has_intersection(vars_, mut->free_vars())) {
            if (auto var = mut->has_var()) vars_ = vars_.insert(var);
            return Rewriter::rewrite_mut(mut);
        }
        return map(mut, mut);
    }

private:
    Vars vars_;
};

} // namespace mim
