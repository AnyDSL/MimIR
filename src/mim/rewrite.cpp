#include "mim/rewrite.h"

#include <absl/container/fixed_array.h>

#include "mim/check.h"
#include "mim/world.h"

// Don't use fancy C++-lambdas; it's way too annoying stepping through them in a debugger.

namespace mim {

const Def* Rewriter::rewrite(const Def* old_def) {
    if (old_def->isa<Univ>()) return world().univ();
    if (auto new_def = lookup(old_def)) return new_def;

    return dispatch(old_def);
}

const Def* Rewriter::dispatch(const Def* old_def) {
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
    for (size_t i = 0; i != size; ++i)
        new_ops[i] = rewrite(old_def->op(i));
    return old_def->rebuild(world(), new_type, new_ops);
}

const Def* Rewriter::rewrite_mut(Def* old_mut) {
    auto new_type = rewrite(old_mut->type());
    auto new_mut  = old_mut->stub(world(), new_type);
    map(old_mut, new_mut);

    if (old_mut->is_set()) {
        for (size_t i = 0, e = old_mut->num_ops(); i != e; ++i)
            new_mut->set(i, rewrite(old_mut->op(i)));
        if (auto new_imm = new_mut->immutabilize()) return map(old_mut, new_imm);
    }

    return new_mut;
}

const Def* Rewriter::rewrite_seq(const Seq* seq) {
    if (!seq->is_set()) {
        auto new_seq = seq->as_mut<Seq>()->stub(world(), rewrite(seq->type()));
        return map(seq, new_seq);
    }

    auto new_shape = rewrite(seq->arity());

    if (auto l = Lit::isa(new_shape); l && *l <= world().flags().scalarize_threshold) {
        auto new_ops = absl::FixedArray<const Def*>(*l);
        for (size_t i = 0, e = *l; i != e; ++i) {
            if (auto var = seq->has_var()) {
                push();
                map(var, world().lit_idx(e, i));
                new_ops[i] = rewrite(seq->body());
                pop();
            } else {
                new_ops[i] = rewrite(seq->body());
            }
        }
        return map(seq, seq->prod(world(), new_ops));
    }

    if (!seq->has_var()) return map(seq, seq->rebuild(world(), new_shape, rewrite(seq->body())));
    return rewrite_mut(seq->as_mut());
}

const Def* Rewriter::rewrite_extract(const Extract* ex) {
    auto new_index = rewrite(ex->index());
    if (auto index = Lit::isa(new_index)) {
        if (auto tuple = ex->tuple()->isa<Tuple>()) return map(ex, rewrite(tuple->op(*index)));
        if (auto pack = ex->tuple()->isa_imm<Pack>(); pack && pack->arity()->is_closed())
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
