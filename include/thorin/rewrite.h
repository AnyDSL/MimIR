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

/// Stops rewriting when leaving the Scope.
class ScopeRewriter : public Rewriter {
public:
    ScopeRewriter(const Scope& scope)
        : Rewriter(scope.world())
        , scope_(scope) {}

    const Scope& scope() const { return scope_; }

    Ref rewrite(Ref old_def) override {
        if (Infer::should_eliminate(old_def) || scope_.bound(old_def)) return Rewriter::rewrite(old_def);
        return old_def;
    }

private:
    const Scope& scope_;
};

/// Stops rewriting when leaving the Scope.
class VarRewriter : public Rewriter {
public:
    VarRewriter(Ref var, Ref subst)
        : Rewriter(var->world()) {
        if (var) {
            if (auto w = var->isa<Var>()) {
                vars_.emplace(w);
                map(w, subst);
            } else {
                assert(var == subst);
            }
        }
    }

    Ref rewrite_mut(Def* mut) override {
        if (mut->sym().str() == "pow_else") outln("hey");
        if (descend(mut)) {
            if (auto var = mut->has_var()) {
                vars_.emplace(var);
                return Rewriter::rewrite_mut(mut);
            }
        }
        return map(mut, mut);
    }

    bool descend(Def* mut) const {
        for (auto op : mut->extended_ops()) {
            auto fvs = op->free_vars();
            for (auto var : vars_)
                if (fvs.contains(var)) return true;
        }
        return false;
    }

private:
    VarSet vars_;
};

/// @name rewrite
///@{
/// Rewrites @p def by mapping @p old_def to @p new_def while obeying @p scope.
Ref rewrite(Ref def, Ref old_def, Ref new_def, const Scope& scope);

/// Rewrites @p mut's @p i^th op by substituting @p mut's @p Var with @p arg while obeying @p mut's @p scope.
Ref rewrite(Def* mut, Ref arg, size_t i);

/// Same as above but uses @p scope as an optimization instead of computing a new Scope.
Ref rewrite(Def* mut, Ref arg, size_t i, const Scope& scope);

/// Rewrites @p mut's ops by substituting @p mut's @p Var with @p arg while obeying @p mut's @p scope.
DefVec rewrite(Def* mut, Ref arg);

/// Same as above but uses @p scope as an optimization instead of computing a new Scope.
DefVec rewrite(Def* mut, Ref arg, const Scope& scope);
///@}

} // namespace thorin
