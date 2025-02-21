#pragma once

#include "mim/def.h"
#include "mim/rewrite.h"

#include "mim/analyses/scope.h"
#include "mim/pass/pass.h"

namespace mim {

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

    /// @name Getters
    ///@{
    World& world() { return world_; }
    std::string_view name() const { return name_; }
    bool is_dirty() const { return dirty_; }
    ///@}

    /// @name run
    ///@{
    virtual void run(); ///< Entry point and generates some debug output; invokes Phase::start.

    /// Runs a single Phase.
    template<class P, class... Args> static void run(Args&&... args) {
        P p(std::forward<Args>(args)...);
        p.run();
    }
    ///@}

protected:
    virtual void start() = 0; ///< Actual entry.
    void set_name(std::string name) { name_ = name; }

private:
    World& world_;
    std::string name_;
    bool dirty_;
};

/// Visits the current Phase::world and constructs a new RWPhase::world along the way.
/// It recursively **rewrites** all World::externals().
/// @note You can override Rewriter::rewrite, Rewriter::rewrite_imm, and Rewriter::rewrite_mut.
class RWPhase : public Phase, public Rewriter {
public:
    RWPhase(World& world, std::string_view name)
        : Phase(world, name, true)
        , Rewriter(world) {}

    World& world() { return Phase::world(); }
    void start() override;
};

/// Removes unreachable and dead code by rebuilding the whole World into a new one and `swap`ping afterwards.
class Cleanup : public Phase {
public:
    Cleanup(World& world)
        : Phase(world, "cleanup", false) {}

    void start() override;
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
template<class P> class PassPhase : public Phase {
public:
    template<class... Args>
    PassPhase(World& world, Args&&... args)
        : Phase(world, {}, false)
        , man_(world) {
        man_.template add<P>(std::forward<Args>(args)...);
        set_name(std::string(man_.passes().back()->name()) + ".pass_phase");
    }

    void start() override { man_.run(); }

private:
    PassMan man_;
};

/// Wraps a PassMan pipeline as a Phase.
class PassManPhase : public Phase {
public:
    PassManPhase(World& world, std::unique_ptr<PassMan>&& man)
        : Phase(world, "pass_man_phase", false)
        , man_(std::move(man)) {}

    void start() override { man_->run(); }

private:
    std::unique_ptr<PassMan> man_;
};

/// Organizes several Phase%s as a pipeline.
class Pipeline : public Phase {
public:
    Pipeline(World& world)
        : Phase(world, "pipeline", true) {}

    void start() override;

    /// @name phases
    ///@{
    const auto& phases() const { return phases_; }

    /// Add a Phase.
    /// You don't need to pass the World to @p args - it will be passed automatically.
    /// If @p P is a Pass, this method will wrap this in a PassPhase.
    template<class P, class... Args> auto add(Args&&... args) {
        if constexpr (std::is_base_of_v<Pass, P>) {
            return add<PassPhase<P>>(std::forward<Args>(args)...);
        } else {
            auto p     = std::make_unique<P>(world(), std::forward<Args&&>(args)...);
            auto phase = p.get();
            phases_.emplace_back(std::move(p));
            if (phase->is_dirty()) phases_.emplace_back(std::make_unique<Cleanup>(world()));
            return phase;
        }
    }
    ///@}

private:
    std::deque<std::unique_ptr<Phase>> phases_;
};

/// Transitively visits all *reachable* Scope%s in World that do not have free variables.
/// We call these Scope%s *top-level* Scope%s.
/// Select with `elide_empty` whether you want to visit trivial Scope%s of *muts* without body.
/// Assumes that you don't change anything - hence `dirty` flag is set to `false`.
/// @deprecated Use ClosedMutPhase instead.
class ScopePhase : public Phase {
public:
    ScopePhase(World& world, std::string_view name, bool dirty, bool elide_empty)
        : Phase(world, name, dirty)
        , elide_empty_(elide_empty) {}

    void start() override;
    virtual void visit(const Scope&) = 0;

protected:
    const Scope& scope() const { return *scope_; }

private:
    bool elide_empty_;
    const Scope* scope_ = nullptr;
};

/// Transitively visits all *reachable* closed mutables (Def::is_closed()) in World.
/// Select with `elide_empty` whether you want to visit trivial *muts* without body.
/// Assumes that you don't change anything - hence `dirty` flag is set to `false`.
/// If you a are only interested in specific mutables, you can pass this to @p M.
template<class M = Def> class ClosedMutPhase : public Phase {
public:
    ClosedMutPhase(World& world, std::string_view name, bool dirty, bool elide_empty)
        : Phase(world, name, dirty)
        , elide_empty_(elide_empty) {}

    void start() override {
        unique_queue<MutSet> queue;
        for (const auto& [_, mut] : world().externals()) queue.push(mut);

        while (!queue.empty()) {
            auto mut = queue.pop();
            if (auto m = mut->isa<M>(); m && m->is_closed() && (!elide_empty_ || m->is_set())) visit(root_ = m);

            for (auto op : mut->extended_ops())
                for (auto mut : op->local_muts()) queue.push(mut);
        }
    }

protected:
    virtual void visit(M*) = 0;
    M* root() const { return root_; }

private:
    bool elide_empty_;
    M* root_;
};

/// Like ClosedMutPhase but computes a Nest for each NestPhase::visit.
template<class M = Def> class NestPhase : public ClosedMutPhase<M> {
public:
    NestPhase(World& world, std::string_view name, bool dirty, bool elide_empty)
        : ClosedMutPhase<M>(world, name, dirty, elide_empty) {}

    const Nest& nest() const { return *nest_; }
    virtual void visit(const Nest&) = 0;

protected:
    void visit(M* mut) override {
        Nest nest(mut);
        nest_ = &nest;
        visit(nest);
    }

private:
    const Nest* nest_;
};

} // namespace mim
