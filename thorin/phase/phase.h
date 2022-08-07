#pragma once

#include "thorin/def.h"
#include "thorin/rewrite.h"

namespace thorin {

class World;

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
    class Rewriter : public thorin::Rewriter {
    public:
        Rewriter(RewritePhase& phase, World& old_world, World& new_world)
            : thorin::Rewriter(old_world, new_world)
            , phase(phase) {}

        std::pair<const Def*, bool> pre_rewrite(const Def* old_def) override { return phase.pre_rewrite(old_def); }
        std::pair<const Def*, bool> post_rewrite(const Def* old_def) override { return phase.post_rewrite(old_def); }

        RewritePhase& phase;
    };

    RewritePhase(World& world, const char* name)
        : Phase(world, name)
        , new_world_(world.state())
        , rewriter_(*this, world, new_world_) {}

    void start() override;

    /// @name getters
    ///@{
    World& old_world() { return world(); }
    World& new_world() { return new_world_; }
    Def2Def& old2new() { return rewriter_.old2new; }
    ///@}

    /// @name rewrite
    ///@{

    /// See thorin::Rewriter::rewrite.
    const Def* rewrite(const Def* old_def) { return rewriter_.rewrite(old_def); }
    /// See thorin::Rewriter::pre_rewrite.
    virtual std::pair<const Def*, bool> pre_rewrite(const Def*) { return {nullptr, false}; }
    /// See thorin::Rewriter::post_rewrite.
    virtual std::pair<const Def*, bool> post_rewrite(const Def*) { return {nullptr, false}; }
    ///@}

protected:
    World new_world_;
    Rewriter rewriter_;
};

class Cleanup : public RewritePhase {
public:
    Cleanup(World& world)
        : RewritePhase(world, "cleanup") {}
};

} // namespace thorin
