#pragma once

#include "mim/world.h"

namespace mim {

/// Recurseivly rebuilds part of a program **into** the provided World.
/// This World may be different than the World we started with.
class Subst {
public:
    Subst(World& world)
        : world_(world) {}

    World& world() { return world_; }
    /// Map @p old_def to @p new_def and returns @p new_def;
    const Def* map(const Def* old_def, const Def* new_def) { return old2new_[old_def] = new_def; }

    /// @name rewrite
    /// Recursively rewrite old Def%s.
    ///@{
    virtual const Def* subst(const Def*);
    virtual const Def* subst_imm(const Def*);
    virtual const Def* subst_mut(Def*);
    ///@}

private:
    World& world_;
    Def2Def old2new_;
};

class VarSubst : public Subst {
public:
    VarSubst(const Var* var, const Def* arg)
        : Subst(arg->world())
        , vars_(var ? Vars(var) : Vars()) {
        if (var) map(var, arg);
    }

    const Def* subst_imm(const Def* imm) override {
        if (imm->local_vars().empty() && imm->local_muts().empty()) return imm; // safe to skip
        if (imm->has_dep(Dep::Hole) || vars_.has_intersection(imm->free_vars())) return Subst::subst_imm(imm);
        return imm;
    }

    const Def* subst_mut(Def* mut) override {
        if (vars_.has_intersection(mut->free_vars())) {
            if (auto var = mut->has_var()) vars_ = world().vars().insert(vars_, var);
            return Subst::subst_mut(mut);
        }
        return map(mut, mut);
    }

private:
    Vars vars_;
};

} // namespace mim
