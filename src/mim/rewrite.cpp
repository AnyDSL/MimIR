#include "mim/rewrite.h"

#include <absl/container/fixed_array.h>

#include "mim/check.h"
#include "mim/world.h"

// Don't use fancy C++-lambdas; it's way too annoying stepping through them in a debugger.

namespace mim {

const Def* Rewriter::rewrite(const Def* old_def) {
    if (old_def->isa<Univ>()) return world().univ();
    if (auto i = old2new_.find(old_def); i != old2new_.end()) return i->second;

    // clang-format off
    if (auto arr     = old_def->isa<Arr     >()) return rewrite_arr    (arr    );
    if (auto pack    = old_def->isa<Pack    >()) return rewrite_pack   (pack   );
    if (auto extract = old_def->isa<Extract >()) return rewrite_extract(extract);
    if (auto hole    = old_def->isa_mut<Hole>()) return rewrite_hole   (hole   );
    // clang-format on

    if (auto old_mut = old_def->isa_mut()) return rewrite_mut(old_mut);
    return map(old_def, rewrite_imm(old_def));
}

const Def* Rewriter::rewrite_imm(const Def* old_def) {
    auto new_type = old_def->isa<Type>() ? nullptr : rewrite(old_def->type());
    auto size     = old_def->num_ops();
    auto new_ops  = absl::FixedArray<const Def*>(size);
    for (size_t i = 0; i != size; ++i) new_ops[i] = rewrite(old_def->op(i));
    return old_def->rebuild(world(), new_type, new_ops);
}

const Def* Rewriter::rewrite_mut(Def* old_mut) {
    auto new_type = rewrite(old_mut->type());
    auto new_mut  = old_mut->stub(world(), new_type);
    map(old_mut, new_mut);

    if (old_mut->is_set()) {
        for (size_t i = 0, e = old_mut->num_ops(); i != e; ++i) new_mut->set(i, rewrite(old_mut->op(i)));
        if (auto new_imm = new_mut->immutabilize()) return map(old_mut, new_imm);
    }

    return new_mut;
}

const Def* Rewriter::rewrite_arr(const Arr* arr) {
    if (!arr->is_set()) {
        auto new_arr = world().mut_arr(rewrite(arr->type()))->set(arr->dbg());
        return map(arr, new_arr);
    }

    auto new_shape = rewrite(arr->shape());

    if (auto l = Lit::isa(new_shape); l && *l <= world().flags().scalarize_threshold) {
        auto new_ops = absl::FixedArray<const Def*>(*l);
        for (size_t i = 0, e = *l; i != e; ++i) {
            if (auto var = arr->has_var()) {
                auto old2new = old2new_;
                map(var, world().lit_idx(e, i));
                new_ops[i] = rewrite(arr->body());
                old2new_   = std::move(old2new);
            } else {
                new_ops[i] = rewrite(arr->body());
            }
        }
        return map(arr, world().sigma(new_ops));
    }

    if (!arr->has_var()) return map(arr, world().arr(new_shape, rewrite(arr->body()))->set(arr->dbg()));
    return rewrite_mut(arr->as_mut());
}

const Def* Rewriter::rewrite_pack(const Pack* pack) {
    if (!pack->is_set()) {
        auto new_pack = world().mut_arr(rewrite(pack->type()))->set(pack->dbg());
        return map(pack, new_pack);
    }

    auto new_shape = rewrite(pack->shape());

    if (auto l = Lit::isa(new_shape); l && *l <= world().flags().scalarize_threshold) {
        auto new_ops = absl::FixedArray<const Def*>(*l);
        for (size_t i = 0, e = *l; i != e; ++i) {
            if (auto var = pack->has_var()) {
                auto old2new = old2new_;
                map(var, world().lit_idx(e, i));
                new_ops[i] = rewrite(pack->body());
                old2new_   = std::move(old2new);
            } else {
                new_ops[i] = rewrite(pack->body());
            }
        }
        return world().tuple(new_ops);
    }

    if (!pack->has_var()) return map(pack, world().pack(new_shape, rewrite(pack->body()))->set(pack->dbg()));
    return rewrite_mut(pack->as_mut());
}

const Def* Rewriter::rewrite_extract(const Extract* ex) {
    auto new_index = rewrite(ex->index());
    if (auto index = Lit::isa(new_index)) {
        if (auto tuple = ex->tuple()->isa<Tuple>()) return map(ex, rewrite(tuple->op(*index)));
        if (auto pack = ex->tuple()->isa_imm<Pack>(); pack && pack->shape()->is_closed())
            return map(ex, rewrite(pack->body()));
    }

    auto new_tuple = rewrite(ex->tuple());
    return map(ex, world().extract(new_tuple, new_index)->set(ex->dbg()));
}

const Def* Rewriter::rewrite_hole(Hole* hole) {
    auto [last, op] = hole->find();
    return op ? rewrite(op) : rewrite_mut(last);
}

} // namespace mim
