#pragma once

#include <memory>

#include "mim/check.h"
#include "mim/def.h"
#include "mim/lam.h"
#include "mim/lattice.h"
#include "mim/rule.h"
#include "mim/tuple.h"

namespace mim {

class World;

/// Recurseivly rebuilds part of a program **into** the provided World w.r.t.\ Rewriter::map.
/// This World may be different than the World we started with.
class Rewriter {
public:
    Rewriter(std::unique_ptr<World>&& ptr)
        : ptr_(std::move(ptr))
        , world_(ptr_.get()) {
        push(); // create root map
    }

    Rewriter(World& world)
        : world_(&world) {
        push(); // create root map
    }

    void reset(std::unique_ptr<World>&& ptr) {
        ptr_   = std::move(ptr);
        world_ = ptr_.get();
        reset();
    }

    void reset() {
        pop();
        assert(old2news_.empty());
        push();
    }

    World& world() { return *world_; }

    /// @name Stack of Maps
    ///@{
    virtual void push() { old2news_.emplace_back(Def2Def{}); }
    virtual void pop() { old2news_.pop_back(); }

    /// Map @p old_def to @p new_def and returns @p new_def.
    /// @returns `new_def`
    // clang-format off
    const Def* map(const Def* old_def , const Def* new_def ) { return old2news_.back()[old_def] = new_def; }
    const Def* map(const Def* old_def ,       Defs new_defs);
    const Def* map(Defs       old_defs, const Def* new_def );
    const Def* map(Defs       old_defs,       Defs new_defs);
    // clang-format on

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

    friend void swap(Rewriter& rw1, Rewriter& rw2) {
        using std::swap;
        // clang-format off
        swap(rw1.ptr_,      rw2.ptr_);
        swap(rw1.world_,    rw2.world_);
        swap(rw1.old2news_, rw2.old2news_);
        // clang-format on
    }

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

    const Def* rewrite_mut(Def*) final;

    friend void swap(VarRewriter& vrw1, VarRewriter& vrw2) {
        using std::swap;
        swap(static_cast<Rewriter&>(vrw1), static_cast<Rewriter&>(vrw2));
        swap(vrw1.vars_, vrw2.vars_);
    }

private:
    bool has_intersection(const Def* old_def) {
        for (const auto& vars : vars_ | std::views::reverse)
            if (vars.has_intersection(old_def->free_vars())) return true;
        return false;
    }

    Vector<Vars> vars_;
};

class Zonker : public Rewriter {
public:
    Zonker(World& world)
        : Rewriter(world) {}

    const Def* rewrite(const Def* def) final {
        auto res = def;
        while (res->zonked_)
            res = res->zonked_;
        auto last = res;

        if (auto hole = res->isa_mut<Hole>()) {
            auto [last, op] = hole->find();
            res             = op ? rewrite(op) : last;
        }

        if (res->needs_zonk()) res = Rewriter::rewrite(res);

        // path compression
        for (auto d = def; d != last;) {
            auto next  = d->zonked_;
            d->zonked_ = res;
            d          = next;
        }

        return res;
    }

    const Def* rewrite_mut(Def* root) final {
        // Don't create a new stub, instead rewrire the ops of the old mutable root.
        map(root, root);

        auto old_type = root->type();
        auto old_ops  = absl::FixedArray<const Def*>(root->ops().begin(), root->ops().end());

        root->unset()->set_type(rewrite(old_type));

        for (size_t i = 0, e = root->num_ops(); i != e; ++i)
            root->set(i, rewrite(old_ops[i]));
        if (auto new_imm = root->immutabilize()) return map(root, new_imm);

        return root;
    }

    friend void swap(Zonker& z1, Zonker& z2) {
        using std::swap;
        swap(static_cast<Rewriter&>(z1), static_cast<Rewriter&>(z2));
    }
};

} // namespace mim
