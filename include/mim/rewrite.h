#pragma once

#include <memory>

#include "mim/world.h"

namespace mim {

/// Recurseivly rebuilds part of a program **into** the provided World w.r.t.\ Rewriter::map.
/// This World may be different than the World we started with.
class Rewriter {
public:
    /// @name Constructoin
    ///@{
    Rewriter(World& world)
        : world_(&world) {
        push(); // create root map
    }
    Rewriter() { push(); /* create root map */ } ///< Rewrite::set World later.

    void set(std::unique_ptr<World>&& ptr) {
        ptr_   = std::move(ptr);
        world_ = ptr_.get();
    }
    ///@}

    World& world() { return *world_; }

    /// @name Stack of Maps
    ///@{
    virtual void push() { old2news_.emplace_back(Def2Def{}); }
    virtual void pop() { old2news_.pop_back(); }

    /// Map @p old_def to @p new_def and returns @p new_def.
    /// @returns `new_def`
    const Def* map(const Def* old_def, const Def* new_def) { return old2news_.back()[old_def] = new_def; }

    /// Lookup `old_def` by searching in reverse through the stack of maps.
    /// @returns `nullptr` if nothing was found.
    const Def* lookup(const Def* old_def) {
        for (const auto& old2new : old2news_ | std::views::reverse)
            if (auto i = old2new.find(old_def); i != old2new.end()) return i->second;
        return nullptr;
    }
    ///@}

    /// @name rewrite
    /// Recursively rewrite old Def%s.
    ///@{
    virtual const Def* rewrite(const Def*);
    virtual const Def* rewrite_imm(const Def*);
    virtual const Def* rewrite_mut(Def*);
    virtual const Def* rewrite_stub(Def*, Def*);
    virtual DefVec rewrite(Defs);

#define CODE_IMM(N) virtual const Def* rewrite_imm_##N(const N*);
#define CODE_MUT(N) virtual const Def* rewrite_mut_##N(N*);
    MIM_IMM_NODE(CODE_IMM)
    MIM_MUT_NODE(CODE_MUT)
#undef CODE_IMM
#undef CODE_MUT

    virtual const Def* rewrite_imm_Seq(const Seq* seq);
    virtual const Def* rewrite_mut_Seq(Seq* seq);
    ///@}

private:
    std::unique_ptr<World> ptr_;
    World* world_;
    std::deque<Def2Def> old2news_;
};

class VarRewriter : public Rewriter {
public:
    VarRewriter(World& world)
        : Rewriter(world) {}

    VarRewriter(const Var* var, const Def* arg)
        : Rewriter(arg->world()) {
        add(var, arg);
    }

    void add(const Var* var, const Def* arg) {
        map(var, arg);
        vars_.emplace_back(var);
    }

    void push() final {
        Rewriter::push();
        vars_.emplace_back(Vars());
    }

    void pop() final {
        vars_.pop_back();
        Rewriter::pop();
    }

    const Def* rewrite(const Def* old_def) final {
        if (auto new_def = lookup(old_def)) return new_def;

        if (auto old_mut = old_def->isa_mut())
            return has_intersection(old_mut) ? rewrite_mut(old_mut)->set(old_mut->dbg()) : old_mut;

        if (old_def->local_vars().empty() && old_def->local_muts().empty()) return old_def; // safe to skip

        return has_intersection(old_def) ? rewrite_imm(old_def)->set(old_def->dbg()) : old_def;
    }

    const Def* rewrite_mut(Def* mut) final {
        if (auto var = mut->has_var()) {
            auto& vars = vars_.back();
            vars       = world().vars().insert(vars, var);
        }

        return Rewriter::rewrite_mut(mut);
    }

private:
    bool has_intersection(const Def* old_def) {
        for (const auto& vars : vars_ | std::views::reverse)
            if (vars.has_intersection(old_def->free_vars())) return true;
        return false;
    }

    Vector<Vars> vars_;
};

} // namespace mim
