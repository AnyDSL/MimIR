#include "mim/rewrite.h"

#include <absl/container/fixed_array.h>

#include "mim/check.h"
#include "mim/world.h"

// Don't use fancy C++-lambdas; it's way too annoying stepping through them in a debugger.

namespace mim {

const Def* Rewriter::rewrite(const Def* old_def) {
    old_def = Hole::find(old_def);
    if (old_def->isa<Univ>()) return world().univ();
    if (auto i = old2new_.find(old_def); i != old2new_.end()) return i->second;
    if (auto old_mut = old_def->isa_mut()) return rewrite_mut(old_mut);
    return map(old_def, rewrite_imm(old_def));
}

const Def* Rewriter::rewrite_imm(const Def* old_def) {
    // Extracts are used as conditional branches: make sure that we don't rewrite unreachable stuff.
    if (auto extract = old_def->isa<Extract>()) {
        if (auto index = Lit::isa(rewrite(extract->index()))) {
            if (auto tuple = extract->tuple()->isa<Tuple>()) return rewrite(tuple->op(*index));
            if (auto pack = extract->tuple()->isa_imm<Pack>(); pack && pack->shape()->is_closed())
                return rewrite(pack->body());
        }
    }

    auto new_type = old_def->isa<Type>() ? nullptr : rewrite(old_def->type());
    auto size     = old_def->num_ops();
    auto new_ops  = absl::FixedArray<const Def*>(size);
    for (size_t i = 0; i != size; ++i) new_ops[i] = rewrite(old_def->op(i));
    return old_def->rebuild(world(), new_type, new_ops);
}

const Def* Rewriter::rewrite_mut(Def* old_mut) {
    auto new_type = rewrite(old_mut->type());

    if (auto arr = old_mut->isa<Arr>()) {
        if (auto l = Lit::isa(rewrite(arr->shape())); l && *l <= world().flags().scalarize_threshold) {
            auto new_ops = absl::FixedArray<const Def*>(*l);
            for (size_t i = 0, e = *l; i != e; ++i) {
                if (auto var = arr->has_var()) {
                    auto old2new = old2new_;
                    map(var, world().lit_idx(e, i));
                    new_ops[i] = rewrite(arr->body());
                    old2new_   = std::move(old2new);
                } else {
                    new_ops[i] = arr->body();
                }
            }
            return world().sigma(new_ops);
        }
    }

    // HACK copy & past from above
    if (auto pack = old_mut->isa<Pack>()) {
        if (auto l = Lit::isa(rewrite(pack->shape())); l && *l <= world().flags().scalarize_threshold) {
            auto new_ops = absl::FixedArray<const Def*>(*l);
            for (size_t i = 0, e = *l; i != e; ++i) {
                if (auto var = pack->has_var()) {
                    auto old2new = old2new_;
                    map(var, world().lit_idx(e, i));
                    new_ops[i] = rewrite(pack->body());
                    old2new_   = std::move(old2new);
                } else {
                    new_ops[i] = pack->body();
                }
            }
            return world().tuple(new_ops);
        }
    }

    auto new_mut = old_mut->stub(world(), new_type);
    map(old_mut, new_mut);

    if (old_mut->is_set()) {
        for (size_t i = 0, e = old_mut->num_ops(); i != e; ++i) new_mut->set(i, rewrite(old_mut->op(i)));
        if (auto new_imm = new_mut->immutabilize()) return map(old_mut, new_imm);
    }

    return new_mut;
}

} // namespace mim
