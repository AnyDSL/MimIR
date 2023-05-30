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

    /// @name recursively rewrite old Defs
    ///@{
    Ref map(Ref old_def, Ref new_def) { return old2new_[old_def] = new_def; }
    virtual Ref rewrite(Ref);
    virtual Ref rewrite_imm(Ref);
    virtual Ref rewrite_mut(Def*);
    ///@}

protected:
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
        if (!old_def || !scope().bound(old_def)) return old_def;
        return Rewriter::rewrite(old_def);
    }

private:
    const Scope& scope_;
};

class InferRewriter : public Rewriter {
public:
    InferRewriter(World& world)
        : Rewriter(world) {}

    Ref rewrite(Ref old_def) override {
        if (!old_def || old_def->isa_mut()) return old_def;
        if (old_def->has_dep(Dep::Infer)) return Rewriter::rewrite(old_def);
        return old_def;
    }
};

class EvalRewriter : public Rewriter {
public:
    EvalRewriter(World& world)
        : Rewriter(world) {}

    Ref rewrite_imm(Ref def) override {
        if (!def || def->dep_const()) return def;

        if (auto var = def->isa<Var>()) {
            if (auto i = old2new_.find(var->mut()); i != old2new_.end()) {
                if (auto mut = i->second->isa_mut()) {
                    if (auto new_var = mut->var()) return new_var;
                }
            }
            return var;
        } else if (auto app = def->isa<App>()) {
            auto arg = rewrite(app->arg());
            if (auto lam = app->callee()->isa_mut<Lam>(); lam && lam->is_set()) return lam->reduce(arg).back();
        }

        return Rewriter::rewrite_imm(def);
    }
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
DefArray rewrite(Def* mut, Ref arg);

/// Same as above but uses @p scope as an optimization instead of computing a new Scope.
DefArray rewrite(Def* mut, Ref arg, const Scope& scope);

/// Evaluates @p def via call-by-value, left to right.
Ref eval(Ref def);
///@}

} // namespace thorin
