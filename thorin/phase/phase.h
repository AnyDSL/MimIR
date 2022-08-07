#pragma once

#include "thorin/def.h"
#include "thorin/rewrite.h"

#include "thorin/pass/pass.h"

namespace thorin {

class World;

/// As opposed to a Pass, a Phase does one thing at a time and does not mix with other Phase%s.
/// They are supposed to classically run one after another.
/// Phase::dirty indicates whether we may need a Cleanup afterwards.
class Phase {
public:
    Phase(World& world, std::string_view name, bool dirty)
        : world_(world)
        , name_(name)
        , dirty_(dirty) {}
    virtual ~Phase() = default;

    /// @name getters
    ///@{
    World& world() { return world_; }
    std::string_view name() const { return name_; }
    bool is_dirty() const { return dirty_; }
    ///@}

    /// @name run/start
    ///@{
    virtual void run();       ///< Entry point and generates some debug output; invokes Phase:start.
    virtual void start() = 0; ///< Actual entry.
    ///@}

    /// Runs a single Phase.
    template<class P, class... Args>
    static void run(Args&&... args) {
        P p(std::forward<Args>(args)...);
        p.run();
    }

protected:
    World& world_;
    std::string name_;
    bool dirty_;
};

class RWPhase;

class PhaseRewriter : public Rewriter {
public:
    PhaseRewriter(RWPhase& phase, World& old_world, World& new_world)
        : Rewriter(old_world, new_world)
        , phase(phase) {}

    std::pair<const Def*, bool> pre_rewrite(const Def*) override;
    std::pair<const Def*, bool> post_rewrite(const Def*) override;

    RWPhase& phase;
};

/// Visits the current Phase::world and constructs a new World Phase::new_world along the way.
/// It recursively **rewrites** all World::externals().
class RWPhase : public Phase {
public:
    RWPhase(World& world, std::string_view name)
        : Phase(world, name, false)
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
    PhaseRewriter rewriter_;
};

class Cleanup : public RWPhase {
public:
    Cleanup(World& world)
        : RWPhase(world, "cleanup") {}
};

/// Like a RWPhase but starts with a fixed-point loop of FPPhase::analyze beforehand.
/// Inherit from this one to implement a classic data-flow analysis.
class FPPhase : public RWPhase {
public:
    FPPhase(World& world, std::string_view name)
        : RWPhase(world, name) {}

    void start() override;
    virtual bool analyze() = 0;
};

/// Wraps a Pass as a Phase.
template<class P>
class PassPhase : public Phase {
public:
    template<class... Args>
    PassPhase(World& world, Args&&... args)
        : Phase(world, {}, false)
        , man_(world) {
        man_.template add<P>(std::forward<Args>(args)...);
        name_ = std::string(man_.passes().back()->name()) + ".pass_phase";
    }

    void start() override { man_.run(); }

private:
    PassMan man_;
};

/// Wraps a PassMan pipeline as a Phase.
template<class P>
class PassManPhase : public Phase {
public:
    PassManPhase(World& world, PassMan& man)
        : Phase(world, "pass_man_phase")
        , man_(man) {}

    void start() override { man_.run(); }

private:
    PassMan man_;
};

/// Organizes several Phase%s as a pipeline.
class Pipeline : public Phase {
public:
    Pipeline(World& world)
        : Phase(world, "pipeline", true) {}

    const auto& phases() const { return phases_; }

    void start() override;

    /// Add a Phase.
    template<class P, class... Args>
    auto add(Args&&... args) {
        if constexpr (std::is_base_of_v<Pass, P>) {
            return add<PassPhase<P>>(std::forward<Args>(args)...);
        } else {
            auto p     = std::make_unique<P>(world(), std::forward<Args>(args)...);
            auto phase = p.get();
            phases_.emplace_back(std::move(p));
            if (phase->is_dirty()) phases_.emplace_back(std::make_unique<Cleanup>(world()));
            return phase;
        }
    }

private:
    std::deque<std::unique_ptr<Phase>> phases_;
};

/// Transitively visits all *reachable* Scope%s in World that do not have free variables.
/// We call these Scope%s *top-level* Scope%s.
/// Select with `elide_empty` whether you want to visit trivial Scope%s of *noms* without body.
/// Assumes that you don't change anything - hence `dirty` flag is set to `false`.
class ScopePhase : public Phase {
public:
    ScopePhase(World& world, std::string_view name, bool elide_empty)
        : Phase(world, name, false)
        , elide_empty_(elide_empty) {}

    void start() override;
    virtual void visit(const Scope&) = 0;

protected:
    const Scope& scope() const { return *scope_; }

private:
    bool elide_empty_;
    const Scope* scope_ = nullptr;
};

} // namespace thorin
