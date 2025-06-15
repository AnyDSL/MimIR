#pragma once

#include <type_traits>

#include "mim/world.h"

namespace mim {

/// Recurseivly rebuilds part of a program **into** the provided World w.r.t.\ Rewriter::map.
/// This World may be different than the World we started with.
class Rewriter {
public:
    Rewriter(World& world)
        : world_(world) {}

    World& world() { return world_; }
    /// Map @p old_def to @p new_def and returns @p new_def;
    const Def* map(const Def* old_def, const Def* new_def) { return old2new_[old_def] = new_def; }

    /// @name rewrite
    /// Recursively rewrite old Def%s.
    ///@{
    virtual const Def* rewrite(const Def*);
    virtual const Def* rewrite_imm(const Def*);
    virtual const Def* rewrite_mut(Def*);

    virtual const Def* rewrite_arr(const Arr* arr) { return rewrite_arr_or_pack<true>(arr); }
    virtual const Def* rewrite_pack(const Pack* pack) { return rewrite_arr_or_pack<false>(pack); }
    virtual const Def* rewrite_extract(const Extract*);
    ///@}

private:
    template<bool arr> const Def* rewrite_arr_or_pack(std::conditional_t<arr, const Arr*, const Pack*> pa) {
        auto new_shape = rewrite(pa->shape());

        if (auto l = Lit::isa(new_shape); l && *l <= world().flags().scalarize_threshold) {
            auto new_ops = absl::FixedArray<const Def*>(*l);
            for (size_t i = 0, e = *l; i != e; ++i) {
                if (auto var = pa->has_var()) {
                    auto old2new = old2new_;
                    map(var, world().lit_idx(e, i));
                    new_ops[i] = rewrite(pa->body());
                    old2new_   = std::move(old2new);
                } else {
                    new_ops[i] = rewrite(pa->body());
                }
            }
            return arr ? world().sigma(new_ops) : world().tuple(new_ops);
        }

        if (!pa->has_var()) {
            auto new_body = rewrite(pa->body());
            return arr ? world().arr(new_shape, new_body) : world().pack(new_shape, new_body);
        }

        return rewrite_mut(pa->as_mut());
    }

    World& world_;
    Def2Def old2new_;
};

class VarRewriter : public Rewriter {
public:
    VarRewriter(const Var* var, const Def* arg)
        : Rewriter(arg->world())
        , vars_(var ? Vars(var) : Vars()) {
        assert(var);
        map(var, arg);
    }

    const Def* rewrite_imm(const Def* imm) override {
        if (imm->local_vars().empty() && imm->local_muts().empty()) return imm; // safe to skip
        if (imm->has_dep(Dep::Hole) || vars_.has_intersection(imm->free_vars())) return Rewriter::rewrite_imm(imm);
        return imm;
    }

    const Def* rewrite_mut(Def* mut) override {
        if (vars_.has_intersection(mut->free_vars())) {
            if (auto var = mut->has_var()) vars_ = world().vars().insert(vars_, var);
            return Rewriter::rewrite_mut(mut);
        }
        return map(mut, mut);
    }

private:
    Vars vars_;
};

} // namespace mim
