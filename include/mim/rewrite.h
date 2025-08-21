#pragma once

#include <ranges>
#include <type_traits>

#include "mim/world.h"

namespace mim {

/// Recurseivly rebuilds part of a program **into** the provided World w.r.t.\ Rewriter::map.
/// This World may be different than the World we started with.
class Rewriter {
public:
    Rewriter(World& world)
        : world_(world) {
        push(); // create root map
    }

    World& world() { return world_; }

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

    /// @name rewrite
    /// Recursively rewrite old Def%s.
    ///@{
    virtual const Def* rewrite(const Def*);
    virtual DefVec rewrite(Defs);
    const Def* dispatch(const Def*);

    virtual const Def* rewrite_imm(const Def*);
    virtual const Def* rewrite_mut(Def*);

    // virtual const Def* rewrite_app(const App* app) { return map(app, rewrite_imm(app)); }
    // virtual const Def* rewrite_lam(const Lam* old_lam) {
    //     if (auto old_mut = old_lam->isa_mut()) return rewrite_mut(old_mut);
    //     return map(old_lam, rewrite_imm(old_lam));
    // }
    // virtual const Def* rewrite_extract(const Extract*);
    // virtual const Def* rewrite_hole(Hole*);

#define CODE(N, n, _, mut) virtual const Def* rewrite_##n(std::conditional_t<mut == Mut::Mut, N*, const N*>);
    MIM_NODE(CODE)
#undef CODE

    virtual const Def* rewrite_seq(const Seq*);

    ///@}

private:
    World& world_;
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
        if (old_def->isa<Univ>()) return world().univ();
        if (auto new_def = lookup(old_def)) return new_def;
        if (descend(old_def)) return Rewriter::dispatch(old_def);
        // return map(mut, mut);
        return old_def;
    }

    bool descend(const Def* old_def) {
        if (auto imm = old_def->isa_imm()) {
            if (imm->has_dep(Dep::Hole)) return true;
            if (imm->local_vars().empty() && imm->local_muts().empty()) return false; // safe to skip
        }

        for (const auto& vars : vars_ | std::views::reverse)
            if (vars.has_intersection(old_def->free_vars())) return true;

        return false;
    }

    const Def* rewrite_mut(Def* mut) final {
        if (auto var = mut->has_var()) {
            auto& vars = vars_.back();
            vars       = world().vars().insert(vars, var);
        }

        return Rewriter::rewrite_mut(mut);
    }

private:
    Vector<Vars> vars_;
};

} // namespace mim
