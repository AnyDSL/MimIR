#pragma once

#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

/// Rewrites part of a program.
class Rewriter {
public:
    Rewriter(World& old_world, World& new_world, const Scope* scope = nullptr)
        : old_world(old_world)
        , new_world(new_world)
        , scope(scope) {
        old2new[old_world.univ()] = new_world.univ();
    }
    Rewriter(World& world, const Scope* scope = nullptr)
        : Rewriter(world, world, scope) {}

    const Def* rewrite(const Def* old_def);
    World& world() {
        assert(&old_world == &new_world);
        return old_world;
    }

    World& old_world;
    World& new_world;
    const Scope* scope;
    Def2Def old2new;
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

/// Removes unreachable and dead code by rebuilding the whole @p world into a new World.
void cleanup(World& world);

} // namespace thorin
