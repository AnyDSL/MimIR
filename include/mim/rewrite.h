#pragma once

#include <ranges>

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
    virtual const Def* rewrite_imm(const Def*);
    virtual const Def* rewrite_mut(Def*, bool immutablize);

    virtual const Def* rewrite_arr(const Arr* arr) { return rewrite_seq(arr); }
    virtual const Def* rewrite_pack(const Pack* pack) { return rewrite_seq(pack); }
    virtual const Def* rewrite_seq(const Seq*);
    virtual const Def* rewrite_extract(const Extract*);
    virtual const Def* rewrite_var(const Var*);
    virtual const Def* rewrite_hole(Hole*);
    ///@}

private:
    World& world_;
    std::deque<Def2Def> old2news_;
    MutSet muts_;
};

class VarRewriter : public Rewriter {
public:
    VarRewriter(const Var* var, const Def* arg)
        : Rewriter(arg->world()) {
        assert(var);
        map(var, arg);
        vars_.emplace_back(var);
        Rewriter::push();
    }

    void push() final {
        vars_.emplace_back(Vars());
        Rewriter::push();
    }

    void pop() final {
        Rewriter::pop();
        vars_.pop_back();
    }

    bool descend(const Def* old_def) {
        for (auto& vars : vars_ | std::views::reverse) {
            if (vars.has_intersection(old_def->free_vars())) {
                if (auto var = old_def->has_var()) vars = world().vars().insert(vars, var);
                return true;
            }
        }
        return false;
    }

    const Def* rewrite_imm(const Def* imm) final {
        if (imm->local_vars().empty() && imm->local_muts().empty()) return imm; // safe to skip
        if (imm->has_dep(Dep::Hole) || descend(imm)) return Rewriter::rewrite_imm(imm);
        return imm;
    }

    const Def* rewrite_mut(Def* mut, bool immutablize) final {
        if (descend(mut)) return Rewriter::rewrite_mut(mut, immutablize);
        return map(mut, mut);
    }

public:
    Vector<Vars> vars_;
};

} // namespace mim
