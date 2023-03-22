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
    const Def* map(const Def* old_def, const Def* new_def) { return old2new_[old_def] = new_def; }
    virtual const Def* rewrite(Ref);
    virtual const Def* rewrite_structural(const Def*);
    virtual const Def* rewrite_nom(Def*);
    ///@}

private:
    World& world_;
    Def2Def old2new_;
};

/// Stops rewriting when leaving the Scope.
class ScopeRewriter : public Rewriter {
public:
    ScopeRewriter(World& world, const Scope& scope)
        : Rewriter(world)
        , scope_(scope) {}

    const Scope& scope() const { return scope_; }

    const Def* rewrite(Ref old_def) override {
        if (!old_def || !scope().bound(old_def)) return old_def;
        return Rewriter::rewrite(old_def);
    }

private:
    const Scope& scope_;
};

/// Stops rewriting on nominals.
class DAGRewriter : public Rewriter {
public:
    DAGRewriter(World& world)
        : Rewriter(world) {}

    const Def* rewrite_nom(Def* nom) override { return nom; }
};

/// Rewrites @p def by mapping @p old_def to @p new_def while obeying @p scope.
const Def* rewrite(const Def* def, const Def* old_def, const Def* new_def, const Scope& scope);

/// Rewrites @p nom's @p i^th op by substituting @p nom's @p Var with @p arg while obeying @p nom's @p scope.
const Def* rewrite(Def* nom, const Def* arg, size_t i);

/// Same as above but uses @p scope as an optimization instead of computing a new Scope.
const Def* rewrite(Def* nom, const Def* arg, size_t i, const Scope& scope);

/// Rewrites @p nom's ops by substituting @p nom's @p Var with @p arg while obeying @p nom's @p scope.
DefArray rewrite(Def* nom, const Def* arg);

/// Same as above but uses @p scope as an optimization instead of computing a new Scope.
DefArray rewrite(Def* nom, const Def* arg, const Scope& scope);

} // namespace thorin
