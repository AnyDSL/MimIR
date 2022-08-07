#pragma once

#include "thorin/world.h"

namespace thorin {

/// As opposed to a Pass, a Phase does one thing at a time and does not mix with other Phase%s.
/// They are supposed to classically run one after another.
class Phase {
public:
    Phase(World& world, const char* name)
        : world_(world)
        , name_(name) {}

    /// @name getters
    ///@{
    World& world() { return world_; }
    const char* name() const { return name_; }
    ///@}

    /// @name run/start
    ///@{
    virtual void run();       ///< Entry point and generates some debug output; invokes Phase:start.
    virtual void start() = 0; ///< Actual entry.
    ///@}

protected:
    World& world_;
    const char* name_;
};

/// Visits the current Phase::world and constructs a new World Phase::new_ along the way.
/// It recursively rewrites all World::externals().
class RewritePhase : public Phase {
protected:
    RewritePhase(World& world, const char* name)
        : Phase(world, name)
        , new_(world.state()) {}

    void start() override;

    /// @name hooks
    ///@{

    /// Recursively rewrites @p old_def.
    /// Invokes RewritePhase::pre_rewrite at the beginning and RewritePhase::post_rewrite at the end.
    virtual const Def* rewrite(const Def* old_def);
    /// Rewrite a given Def in pre-order.
    /// @return `{nullptr, false}` to do nothing, `{new_def, false}` to return `new_def` **without** recursing on
    /// `new_def` again, and `{new_def, true}` to return `new_def` **with** recursing on `new_def` again.
    virtual std::pair<const Def*, bool> pre_rewrite(const Def*) { return {nullptr, false}; }
    /// As above but in post-order.
    virtual std::pair<const Def*, bool> post_rewrite(const Def*) { return {nullptr, false}; }
    ///@}

protected:
    World new_;
    Def2Def old2new_;
};

class Cleanup : public RewritePhase {
public:
    Cleanup(World& world)
        : RewritePhase(world, "cleanup") {}
};

} // namespace thorin
