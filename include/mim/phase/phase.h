#pragma once

#include "mim/def.h"
#include "mim/nest.h"
#include "mim/rewrite.h"

#include "mim/pass/pass.h"

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
    Phase(World& world, std::string name, bool dirty)
        : world_(world)
        , name_(std::move(name))
        , dirty_(dirty) {}
    virtual ~Phase() = default;

    virtual void reset() { todo_ = false; }
    ///@}

    /// @name Getters
    ///@{
    World& world() { return world_; }
    std::string_view name() const { return name_; }
    bool is_dirty() const { return dirty_; }
    bool todo() const { return todo_; }
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

private:
    World& world_;
    const std::string name_;
    const bool dirty_;

protected:
    bool todo_ = false; ///< Set to `true` if you want to run all Phase%es in your Pipeline within a fixed-point.
};

/// Rewrites the RWPhase::old_world into the RWPhase::new_world and `swap`s them afterwards.
/// This will destroy RWPhase::old_world leaving RWPhase::new_world as the *current* World to work with.
/// This Phase recursively **rewrites** all (old) World::externals() & World::annexes()
/// @note You can override Rewriter::rewrite, Rewriter::rewrite_imm, Rewriter::rewrite_mut, etc.
class RWPhase : public Phase, public Rewriter {
public:
    /// @name Construction
    ///@{
    RWPhase(World& world, std::string name)
        : Phase(world, std::move(name), false)
        , Rewriter(world.inherit_ptr()) {}

    void reset() override { Phase::reset(), Rewriter::reset(); }
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

/// Removes unreachable and dead code by rebuilding the whole World into a new one and `swap`ping afterwards.
class Cleanup : public RWPhase {
public:
    Cleanup(World& world)
        : RWPhase(world, "cleanup") {}
};

/// Like a RWPhase but starts with a fixed-point loop of FPPhase::analyze beforehand.
/// Inherit from this one to implement a classic data-flow analysis.
/// @note If you don't need a fixed-point, just return `true` after the first run of analyze.
class FPPhase : public RWPhase {
public:
    FPPhase(World& world, std::string name)
        : RWPhase(world, std::move(name)) {}

    virtual bool analyze() = 0;
    void start() override;
};

/// Wraps a Pass as a Phase.
template<class P>
class PassPhase : public Phase {
public:
    template<class... Args>
    PassPhase(World& world, Args&&... args)
        : Phase(world, "pass phase", true)
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
        : Phase(world, "pass_man_phase", true)
        , man_(std::move(man)) {}

    void start() override { man_->run(); }

private:
    std::unique_ptr<PassMan> man_;
};

/// Organizes several Phase%s as a pipeline.
class Pipeline : public Phase {
public:
    Pipeline(World& world)
        : Phase(world, "pipeline", false) {}

    void start() override;

    /// @name phases
    ///@{
    const auto& phases() const { return phases_; }

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
            if (phase->is_dirty()) phases_.emplace_back(std::make_unique<Cleanup>(world()));
            return phase;
        }
    }
    ///@}

    template<class A, class P, class... Args>
    static void hook(Phases& phases, Args&&... args) {
        auto f = [... args = std::forward<Args>(args)](Pipeline& pipe, const Def*) { pipe.template add<P>(args...); };
        assert_emplace(phases, flags_t(Annex::Base<A>), f);
    }

private:
    std::deque<std::unique_ptr<Phase>> phases_;
};

/// Transitively visits all *reachable* closed mutables (Def::is_closed()) in World.
/// Select with `elide_empty` whether you want to visit trivial *muts* without body.
/// Assumes that you don't change anything - hence `dirty` flag is set to `false`.
/// If you a are only interested in specific mutables, you can pass this to @p M.
template<class M = Def>
class ClosedMutPhase : public Phase {
public:
    ClosedMutPhase(World& world, std::string name, bool dirty, bool elide_empty)
        : Phase(world, std::move(name), dirty)
        , elide_empty_(elide_empty) {}

    bool elide_empty() const { return elide_empty_; }

    void start() override {
        unique_queue<MutSet> queue;
        for (auto mut : world().externals())
            queue.push(mut);

        while (!queue.empty()) {
            auto mut = queue.pop();
            if (auto m = mut->isa<M>(); m && m->is_closed() && (!elide_empty_ || m->is_set())) visit(root_ = m);

            for (auto op : mut->deps())
                for (auto mut : op->local_muts())
                    queue.push(mut);
        }
    }

protected:
    virtual void visit(M*) = 0;
    M* root() const { return root_; }

private:
    const bool elide_empty_;
    M* root_;
};

/// Transitively collects all *closed* mutables (Def::is_closed) in a World.
template<class M = Def>
class ClosedCollector : public ClosedMutPhase<M> {
public:
    ClosedCollector(World& world)
        : ClosedMutPhase<M>(world, "closed_collector", false, false) {}

    virtual void visit(M* mut) { muts.emplace_back(mut); }

    /// Wrapper to directly receive all *closed* mutables as Vector.
    static Vector<M*> collect(World& world) {
        ClosedCollector<M> collector(world);
        collector.run();
        return std::move(collector.muts);
    }

    Vector<M*> muts;
};

/// Like ClosedMutPhase but computes a Nest for each NestPhase::visit.
template<class M = Def>
class NestPhase : public ClosedMutPhase<M> {
public:
    NestPhase(World& world, std::string name, bool dirty, bool elide_empty)
        : ClosedMutPhase<M>(world, std::move(name), dirty, elide_empty) {}

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
