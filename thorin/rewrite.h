#pragma once

#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

/// Rewrites part of a program.
class Rewriter {
public:
    Rewriter(World& old_world, World& new_world)
        : old_world(old_world)
        , new_world(new_world) {
        old2new[old_world.univ()] = new_world.univ();
    }
    Rewriter(World& world)
        : Rewriter(world, world) {}

    /// @name rewrite
    ///@{

    /// Recursively rewrites @p old_def.
    /// Invokes Rewriter::pre_rewrite at the beginning and Rewriter::post_rewrite at the end.
    virtual const Def* rewrite(const Def* old_def);

    /// Rewrite a given Def in pre-order.
    /// @return
    /// * `{nullptr, false}` to do nothing
    /// * `{new_def, false}` to return `new_def` **without** recursing on `new_def` again
    /// * `{new_def, true}` to return `new_def` **with** recursing on `new_def` again
    virtual std::pair<const Def*, bool> pre_rewrite(const Def*) { return {nullptr, false}; }
    /// As above but in post-order.
    virtual std::pair<const Def*, bool> post_rewrite(const Def*) { return {nullptr, false}; }
    ///@}

    World& old_world;
    World& new_world;
    Def2Def old2new;
};

class ScopeRewriter : public Rewriter {
public:
    ScopeRewriter(World& world, const Scope& scope)
        : Rewriter(world)
        , scope(scope) {}

    const Def* rewrite(const Def* old_def) override {
        if (!scope.bound(old_def)) return old_def;
        return Rewriter::rewrite(old_def);
    }

    const Scope& scope;
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
