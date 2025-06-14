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

    // clang-format off
    if (auto arr     = old_def->isa<Arr    >()) return rewrite_arr    (arr   ) ;
    if (auto pack    = old_def->isa<Pack   >()) return rewrite_pack   (pack   );
    if (auto extract = old_def->isa<Extract>()) return rewrite_extract(extract);
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

template<bool arr> const Def* Rewriter::rewrite_arr_or_pack(std::conditional_t<arr, const Arr*, const Pack*> pa) {
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

const Def* Rewriter::rewrite_extract(const Extract* extract) {
    auto new_index = rewrite(extract->index());
    if (auto index = Lit::isa(new_index)) {
        if (auto tuple = extract->tuple()->isa<Tuple>()) return rewrite(tuple->op(*index));
        if (auto pack = extract->tuple()->isa_imm<Pack>(); pack && pack->shape()->is_closed())
            return rewrite(pack->body());
    }

    auto new_tuple = rewrite(extract->tuple());
    return world().extract(new_tuple, new_index)->set(extract->dbg());
}

} // namespace mim
