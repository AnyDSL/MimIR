#include "mim/rewrite.h"

#include <absl/container/fixed_array.h>

#include "mim/check.h"
#include "mim/world.h"

#include "fe/assert.h"

// Don't use fancy C++-lambdas; it's way too annoying stepping through them in a debugger.

namespace mim {

const Def* Rewriter::rewrite(const Def* old_def) {
    if (old_def->isa<Univ>()) return world().univ();
    if (auto new_def = lookup(old_def)) return new_def;

    return dispatch(old_def);
}

const Def* Rewriter::dispatch(const Def* old_def) {
    switch (old_def->node()) {
#define CODE(N, n, _, mut)                                                      \
    case Node::N:                                                               \
        if constexpr (mut == Mut::Mut)                                          \
            return rewrite_##n((N*)old_def);                                    \
        else if constexpr (mut == Mut::Imm)                                     \
            return map(old_def, rewrite_##n((N*)old_def)->set(old_def->dbg())); \
        else                                                                    \
            return rewrite_##n((N*)old_def);
        MIM_NODE(CODE)
        default: fe::unreachable();
    }
#undef CODE
}

DefVec Rewriter::rewrite(Defs ops) {
    auto new_ops = DefVec(ops.size());
    for (size_t i = 0, e = ops.size(); i != e; ++i)
        new_ops[i] = rewrite(ops[i]);
    return new_ops;
}

// clang-format off
const Def* Rewriter::rewrite_idx   (const Idx   *  ) { return world().type_idx(); }
const Def* Rewriter::rewrite_nat   (const Nat   *  ) { return world().type_nat(); }
const Def* Rewriter::rewrite_univ  (const Univ  *  ) { return world().univ(); }
const Def* Rewriter::rewrite_app   (const App   * d) { return world().app   (rewrite(d->callee()), rewrite(d->arg())); }
const Def* Rewriter::rewrite_inj   (const Inj   * d) { return world().inj   (rewrite(d->type()), rewrite(d->value())); }
const Def* Rewriter::rewrite_insert(const Insert* d) { return world().insert(rewrite(d->tuple()), rewrite(d->index()), rewrite(d->value())); }
const Def* Rewriter::rewrite_lit   (const Lit   * d) { return world().lit   (rewrite(d->type()), d->get()); }
const Def* Rewriter::rewrite_merge (const Merge * d) { return world().merge (rewrite(d->type()), rewrite(d->ops())); }
const Def* Rewriter::rewrite_proxy (const Proxy * d) { return world().proxy (rewrite(d->type()), d->pass(), d->tag(), rewrite(d->ops())); }
const Def* Rewriter::rewrite_split (const Split * d) { return world().split (rewrite(d->type()), rewrite(d->value())); }
const Def* Rewriter::rewrite_match (const Match * d) { return world().match (rewrite(d->ops())); }
const Def* Rewriter::rewrite_tuple (const Tuple * d) { return world().tuple (rewrite(d->type()), rewrite(d->ops())); }
const Def* Rewriter::rewrite_type  (const Type  * d) { return world().type  (rewrite(d->level())); }
const Def* Rewriter::rewrite_uinc  (const UInc  * d) { return world().uinc  (rewrite(d->op()), d->offset()); }
const Def* Rewriter::rewrite_umax  (const UMax  * d) { return world().umax  (rewrite(d->ops())); }
const Def* Rewriter::rewrite_uniq  (const Uniq  * d) { return world().uniq  (rewrite(d->inhabitant())); }
const Def* Rewriter::rewrite_var   (const Var   * d) { return world().var   (rewrite(d->type()), rewrite(d->mut())->as_mut()); }
const Def* Rewriter::rewrite_top   (const Top   * d) { return world().top   (rewrite(d->type())); }
const Def* Rewriter::rewrite_bot   (const Bot   * d) { return world().bot   (rewrite(d->type())); }
const Def* Rewriter::rewrite_meet  (const Meet  * d) { return world().meet  (rewrite(d->ops())); }
const Def* Rewriter::rewrite_join  (const Join  * d) { return world().join  (rewrite(d->ops())); }
// clang-format on

const Def* Rewriter::rewrite_axm(const Axm* a) {
    if (&a->world() != &world()) {
        auto type    = rewrite(a->type());
        auto new_axm = world().axm(a->normalizer(), a->curry(), a->trip(), type, a->plugin(), a->tag(), a->sub());
        return map(a, new_axm->set(a->dbg()));
    }
    return a;
}

const Def* Rewriter::rewrite_pi(const Pi* pi) {
    if (auto mut_pi = pi->isa_mut<Pi>()) return rewrite_mut(mut_pi);
    return map(pi, world().pi(rewrite(pi->dom()), rewrite(pi->codom()), pi->is_implicit())->set(pi->dbg()));
}

const Def* Rewriter::rewrite_lam(const Lam* lam) {
    if (auto mut_lam = lam->isa_mut<Lam>()) return rewrite_mut(mut_lam);
    auto pi = rewrite(lam->type())->as<Pi>();
    return map(lam, world().lam(pi, rewrite(lam->filter()), rewrite(lam->body()))->set(lam->dbg()));
}

const Def* Rewriter::rewrite_sigma(const Sigma* sigma) {
    if (auto mut_sigma = sigma->isa_mut<Sigma>()) return rewrite_mut(mut_sigma);
    return map(sigma, world().sigma(rewrite(sigma->ops()))->set(sigma->dbg()));
}

const Def* Rewriter::rewrite_arr(const Arr* d) { return rewrite_seq(d); }
const Def* Rewriter::rewrite_pack(const Pack* d) { return rewrite_seq(d); }

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

    auto new_arity = rewrite(seq->arity());

    if (auto l = Lit::isa(new_arity); l && *l <= world().flags().scalarize_threshold) {
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
        return map(seq, world().prod(seq->is_intro(), new_ops));
    }

    if (!seq->has_var()) return map(seq, world().seq(seq->is_intro(), new_arity, rewrite(seq->body())));
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
    // const Def* Rewriter::rewrite_extract(const Extract* d) { return world().extract(rewrite(d->tuple()),
    // rewrite(d->index())); }
}

const Def* Rewriter::rewrite_hole(Hole* hole) {
    auto [last, op] = hole->find();
    return op ? rewrite(op) : rewrite_mut(last);
}

const Def* Rewriter::rewrite_global(Global* global) { return rewrite_mut(global); }

} // namespace mim
