#pragma once

#include <memory>

#include <fe/assert.h>
#include <fe/cast.h>

#include "mim/def.h"
#include "mim/nest.h"
#include "mim/pass.h"
#include "mim/rewrite.h"

namespace mim {

class Nest;
class Phase;
class PhaseMan;
class Repl;
class World;

using Repls  = std::deque<std::unique_ptr<Repl>>;
using Phases = std::deque<std::unique_ptr<Phase>>;

/// As opposed to a Pass, a Phase does one thing at a time and does not mix with other Phase%s.
/// They are supposed to classically run one after another.
class Phase : public Stage {
public:
    /// @name Construction & Destruction
    ///@{
    Phase(World& world, std::string name)
        : Stage(world, name) {}
    Phase(World& world, flags_t annex)
        : Stage(world, annex) {}

    ///@}

    /// @name Getters
    ///@{
    bool todo() const { return todo_; }
    ///@}

    /// @name run
    ///@{
    virtual void run();       ///< Entry point and generates some debug output; invokes Phase::start.
    virtual void start() = 0; ///< Actual entry.

    /// Runs a single Phase.
    template<class P, class... Args>
    static void run(Args&&... args) {
        P p(std::forward<Args>(args)...);
        p.run();
    }
    ///@}

protected:
    /// Set to `true` to indicate that you want to rerun all Phase%es in your current fixed-point PhaseMan.
    bool todo_ = false;
};

/// This Phase will recursively Rewriter::rewrite
/// 1. all World::annexes() (during which Analysis::is_bootstrapping is `true`), and then
/// 2. all World::externals() (during which Analysis::is_bootstrapping is `false`).
/// @note You can override Rewriter::rewrite, Rewriter::rewrite_imm, Rewriter::rewrite_mut, etc.
class Analysis : public Phase, public Rewriter {
public:
    /// @name Construction & Destruction
    ///@{
    Analysis(World& world, std::string name)
        : Phase(world, std::move(name))
        , Rewriter(world) {}
    Analysis(World& world, flags_t annex)
        : Phase(world, annex)
        , Rewriter(world) {}

    /// Clears all members and sets todo() to `false` for next round in a fixed-point iteration.
    /// @sa RWPhase::analyze
    virtual void reset();
    ///@}

    bool is_bootstrapping() const { return bootstrapping_; }

    /// @name Rewrite
    ///@{
    virtual void rewrite_annex(flags_t, const Def*);
    virtual void rewrite_external(Def*);
    ///@}

    /// @name Getters
    ///@{
    World& world() { return Phase::world(); }
    ///@}

protected:
    void start() override;

private:
    bool bootstrapping_ = true;
};

/// Rewrites the RWPhase::old_world into the RWPhase::new_world and `swap`s them afterwards.
/// This will destroy RWPhase::old_world leaving RWPhase::new_world which will be created here as the *current* World to
/// work with.
/// This Phase will recursively Rewriter::rewrite
/// 1. all (old) World::annexes() (during which RWPhase::is_bootstrapping is `true`), and then
/// 2. all (old) World::externals() (during which RWPhase::is_bootstrapping is `false`).
/// @note You can override Rewriter::rewrite, Rewriter::rewrite_imm, Rewriter::rewrite_mut, etc.
class RWPhase : public Phase, public Rewriter {
public:
    /// @name Construction
    ///@{
    RWPhase(World& world, std::string name, Analysis* analysis = nullptr)
        : Phase(world, std::move(name))
        , Rewriter(world.inherit())
        , analysis_(analysis) {}
    RWPhase(World& world, flags_t annex, Analysis* analysis = nullptr)
        : Phase(world, annex)
        , Rewriter(world.inherit())
        , analysis_(analysis) {}
    ///@}

    bool is_bootstrapping() const { return bootstrapping_; }

    /// You can do an optional fixed-point loop on the RWPhase::old_world before rewriting.
    /// If analysis_ is set, use this for the fixed-point loop.
    /// @note If you don't need a fixed-point, just return `false` after the first run of analyze.
    virtual bool analyze();

    /// @name Rewrite
    ///@{
    virtual void rewrite_annex(flags_t, const Def*);
    virtual void rewrite_external(Def*);
    ///@}

    /// @name World
    /// * Phase::world is the **old** one.
    /// * Rewriter::world is the **new** one.
    /// * RWPhase::world is deleted to not confuse this.
    ///@{
    using Phase::world;
    using Rewriter::world;
    World& world() = delete;                         ///< Hides both and forbids direct access.
    World& old_world() { return Phase::world(); }    ///< Get **old** Def%s from here.
    World& new_world() { return Rewriter::world(); } ///< Create **new** Def%s into this.
    ///@}

protected:
    void start() override;

private:
    Analysis* analysis_;
    bool bootstrapping_ = true;
};

/// Simple Stage that searches for a pattern and replaces it.
/// Combine them in a ReplPhase.
class Repl : public Stage {
public:
    Repl(World& world, flags_t annex)
        : Stage(world, annex) {}

    virtual const Def* replace(const Def* def) = 0;
};

class ReplMan : public Repl {
public:
    ReplMan(World& world, flags_t annex)
        : Repl(world, annex) {}

    void apply(Repls&&);
    void apply(const App*) final;
    void apply(Stage& stage) final { apply(std::move(static_cast<ReplMan&>(stage).repls_)); }

    void add(std::unique_ptr<Repl>&& repl) { repls_.emplace_back(std::move(repl)); }
    const auto& repls() const { return repls_; }

private:
    const Def* replace(const Def*) final { fe::unreachable(); }

    Repls repls_;
};

#define MIM_CONCAT_INNER(a, b) a##b
#define MIM_CONCAT(a, b)       MIM_CONCAT_INNER(a, b)

#define MIM_REPL(__stages, __annex, ...) MIM_REPL_IMPL(__stages, __annex, __LINE__, __VA_ARGS__)

// clang-format off
#define MIM_REPL_IMPL(__stages, __annex, __id, ...)                         \
    struct MIM_CONCAT(Repl_, __id) : ::mim::Repl {                          \
        MIM_CONCAT(Repl_, __id)(::mim::World & world, ::mim::flags_t annex) \
            : Repl(world, annex) {}                                         \
                                                                            \
        const ::mim::Def* replace(const ::mim::Def* def) final __VA_ARGS__  \
    };                                                                      \
    ::mim::Stage::hook<__annex, MIM_CONCAT(Repl_, __id)>(__stages)
// clang-format on

class ReplManPhase : public RWPhase {
public:
    /// @name Construction
    ///@{
    ReplManPhase(World& world, std::unique_ptr<ReplMan>&& man)
        : RWPhase(world, "pass_man_phase")
        , man_(std::move(man)) {}
    ReplManPhase(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    void apply(const App*) final;
    void apply(Stage&) final;
    ///@}

    const ReplMan& man() const { return *man_; }

private:
    void start() final;
    const Def* rewrite(const Def*) final;

    std::unique_ptr<ReplMan> man_;
};

/// Removes unreachable and dead code by rebuilding the whole World into a new one and `swap`ping them afterwards.
class Cleanup : public RWPhase {
public:
    Cleanup(World& world)
        : RWPhase(world, "cleanup") {}
    Cleanup(World& world, flags_t annex)
        : RWPhase(world, annex) {}
};

/// Wraps a PassMan pipeline as a Phase.
class PassManPhase : public Phase {
public:
    /// @name Construction
    ///@{
    PassManPhase(World& world, std::unique_ptr<PassMan>&& man)
        : Phase(world, "pass_man_phase")
        , man_(std::move(man)) {}
    PassManPhase(World& world, flags_t annex)
        : Phase(world, annex) {}

    void apply(const App*) final;
    void apply(Stage&) final;
    ///@}

    const PassMan& man() const { return *man_; }

private:
    void start() final { man_->run(); }

    std::unique_ptr<PassMan> man_;
};

/// Organizes several Phase%s in a a pipeline.
/// If @p fixed_point is `true`, run PhaseMan until all Phase%s' Phase::todo_ flags yield `false`.
class PhaseMan : public Phase {
public:
    /// @name Construction
    ///@{
    PhaseMan(World& world, flags_t annex)
        : Phase(world, annex) {}

    void apply(bool, Phases&&);
    void apply(const App*) final;
    void apply(Stage&) final;
    ///@}

    /// @name Getters
    ///@{
    bool fixed_point() const { return fixed_point_; }
    auto& phases() { return phases_; }
    const auto& phases() const { return phases_; }
    ///@}

private:
    void start() final;

    Phases phases_;
    bool fixed_point_;
};

/// Transitively visits all *reachable*, [*closed*](@ref Def::is_closed) mutables in the World.
/// * Select with `elide_empty` whether you want to visit trivial mutables without body.
/// * If you a are only interested in specific mutables, you can pass this to @p M.
template<class M = Def>
class ClosedMutPhase : public Phase {
public:
    ClosedMutPhase(World& world, std::string name, bool elide_empty)
        : Phase(world, std::move(name))
        , elide_empty_(elide_empty) {}
    ClosedMutPhase(World& world, flags_t annex, bool elide_empty)
        : Phase(world, annex)
        , elide_empty_(elide_empty) {}

    bool elide_empty() const { return elide_empty_; }

protected:
    void start() override {
        world().template for_each<M>(elide_empty(), [this](M* mut) { root_ = mut, visit(mut); });
    }
    virtual void visit(M*) = 0;
    M* root() const { return root_; }

private:
    const bool elide_empty_;
    M* root_;
};

/// Like ClosedMutPhase but computes a Nest for each NestPhase::visit.
template<class M = Def>
class NestPhase : public ClosedMutPhase<M> {
public:
    NestPhase(World& world, std::string name, bool elide_empty)
        : ClosedMutPhase<M>(world, std::move(name), elide_empty) {}
    NestPhase(World& world, flags_t annex, bool elide_empty)
        : ClosedMutPhase<M>(world, annex, elide_empty) {}

    const Nest& nest() const { return *nest_; }
    virtual void visit(const Nest&) = 0;

private:
    void visit(M* mut) final {
        Nest nest(mut);
        nest_ = &nest;
        visit(nest);
    }

    const Nest* nest_;
};

} // namespace mim
