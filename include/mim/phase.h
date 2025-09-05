#pragma once

#include <memory>

#include "mim/def.h"
#include "mim/nest.h"
#include "mim/pass.h"
#include "mim/rewrite.h"

#include "fe/cast.h"

namespace mim {

class Nest;
class World;

/// As opposed to a Pass, a Phase does one thing at a time and does not mix with other Phase%s.
/// They are supposed to classically run one after another.
class Phase : public fe::RuntimeCast<Phase> {
public:
    /// @name Construction & Destruction
    ///@{
    Phase(World& world, std::string name)
        : world_(world)
        , name_(std::move(name)) {}
    Phase(World& world, flags_t annex)
        : world_(world)
        , annex_(annex)
        , name_(Annex::demangle(world.driver(), annex)) {}
    virtual ~Phase() = default;

    virtual void set(const App*) {}

    std::unique_ptr<Phase> recreate();
    virtual void inherit(Phase&) {}
    ///@}

    /// @name Getters
    ///@{
    World& world() { return world_; }
    std::string_view name() const { return name_; }
    bool todo() const { return todo_; }
    flags_t annex() const { return annex_; }
    ///@}

    /// @name run
    ///@{
    virtual void run(); ///< Entry point and generates some debug output; invokes Phase::start.

    /// Runs a single Phase.
    template<class P, class... Args>
    static void run(Args&&... args) {
        P p(std::forward<Args>(args)...);
        p.run();
    }
    ///@}

protected:
    virtual void start() = 0; ///< Actual entry.

    /// Set to `true` to indicate that you want to rerun all Phase%es in current your fixed-point PhaseMan.
    bool todo_ = false;

private:
    World& world_;
    flags_t annex_ = 0;
    const std::string name_;
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
    RWPhase(World& world, std::string name)
        : Phase(world, std::move(name))
        , Rewriter(world.inherit_ptr()) {}
    RWPhase(World& world, flags_t annex)
        : Phase(world, annex)
        , Rewriter(world.inherit_ptr()) {}
    ///@}

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

    bool is_bootstrapping() const { return bootstrapping_; }

protected:
    bool bootstrapping_ = true;
    void start() override;
};

/// Removes unreachable and dead code by rebuilding the whole World into a new one and `swap`ping them afterwards.
class Cleanup : public RWPhase {
public:
    Cleanup(World& world)
        : RWPhase(world, "cleanup") {}
    Cleanup(World& world, flags_t annex)
        : RWPhase(world, annex) {}
};

/// Like a RWPhase but starts with a fixed-point loop of FPPhase::analyze beforehand.
/// Inherit from this one to implement a classic data-flow analysis.
/// @note If you don't need a fixed-point, just return `true` after the first run of analyze.
class FPPhase : public RWPhase {
public:
    FPPhase(World& world, std::string name)
        : RWPhase(world, std::move(name)) {}
    FPPhase(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    virtual bool analyze() = 0;
    void start() override;
};

/// Wraps a Pass as a Phase.
template<class P>
class PassPhase : public Phase {
public:
    template<class... Args>
    PassPhase(World& world, Args&&... args)
        : Phase(world, "pass phase")
        , man_(world) {
        man_.template add<P>(std::forward<Args>(args)...);
    }

    void start() override { man_.run(); }

private:
    PassMan man_;
};

/// Wraps a PassMan pipeline as a Phase.
class PassManPhase : public Phase {
public:
    PassManPhase(World& world, std::unique_ptr<PassMan>&& man)
        : Phase(world, "pass_man_phase")
        , man_(std::move(man)) {}

    void start() override { man_->run(); }

private:
    std::unique_ptr<PassMan> man_;
};

/// Organizes several Phase%s in a a pipeline.
/// If @p fixed_point is `true`, run PhaseMan until all Phase%s' Phase::todo_ flags yield `false`.
class PhaseMan : public Phase {
public:
    PhaseMan(World&, bool fixed_point = false);
    void set(const App*) final;

    bool fixed_point() const { return fixed_point_; }
    void start() override;

    /// @name phases
    ///@{
    auto& phases() { return phases_; }
    const auto& phases() const { return phases_; }

#if 0
    /// Add a Phase.
    /// You don't need to pass the World to @p args - it will be passed automatically.
    /// If @p P is a Pass, this method will wrap this in a PassPhase.
    template<class P, class... Args>
    auto add(Args&&... args) {
        if constexpr (std::is_base_of_v<Pass, P>) {
            return add<PassPhase<P>>(std::forward<Args>(args)...);
        } else {
            auto p     = std::make_unique<P>(world(), std::forward<Args>(args)...);
            auto phase = p.get();
            phases_.emplace_back(std::move(p));
            return phase;
        }
    }
#endif
    void add(std::unique_ptr<Phase>&& phase) { phases_.emplace_back(std::move(phase)); }
    ///@}

    template<class A, class P>
    static void hook(Flags2Phases& phases) {
        auto f = [](World& w) { return std::make_unique<P>(w, Annex::Base<A>); };
        assert_emplace(phases, Annex::Base<A>, f);
    }

private:
    std::deque<std::unique_ptr<Phase>> phases_;
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

    void start() override {
        world().template for_each<M>(elide_empty(), [this](M* mut) { root_ = mut, visit(mut); });
    }

protected:
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
