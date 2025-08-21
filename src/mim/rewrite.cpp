#include "mim/rewrite.h"

#include <absl/container/fixed_array.h>

#include "mim/check.h"
#include "mim/world.h"

#include "fe/assert.h"

// Don't use fancy C++-lambdas; it's way too annoying stepping through them in a debugger.

namespace mim {

const Def* Rewriter::rewrite(const Def* old_def) {
    if (auto new_def = lookup(old_def)) return new_def;

    auto new_def = old_def->is_mut() ? rewrite_mut((Def*)old_def) : rewrite_imm(old_def);
    return new_def->set(old_def->dbg());
}

// clang-format off
#define CODE_MUT(N) case Node::N: new_def = rewrite_mut_##N(old_mut->as<N>()); break;
#define CODE_IMM(N) case Node::N: new_def = rewrite_imm_##N(old_def->as<N>()); break;
// clang-format on

const Def* Rewriter::rewrite_imm(const Def* old_def) {
    const Def* new_def;
    switch (old_def->node()) {
        MIM_IMM_NODE(CODE_IMM)
        default: fe::unreachable();
    }
    return map(old_def, new_def);
}

const Def* Rewriter::rewrite_mut(Def* old_mut) {
    const Def* new_def;
    switch (old_mut->node()) {
        MIM_MUT_NODE(CODE_MUT)
        default: fe::unreachable();
    }
    return new_def;
}

#undef CODE_MUT
#undef CODE_IMM

DefVec Rewriter::rewrite(Defs ops) {
    auto new_ops = DefVec(ops.size());
    for (size_t i = 0, e = ops.size(); i != e; ++i)
        new_ops[i] = rewrite(ops[i]);
    return new_ops;
}

// clang-format off
const Def* Rewriter::rewrite_imm_Idx   (const Idx   *  ) { return world().type_idx(); }
const Def* Rewriter::rewrite_imm_Nat   (const Nat   *  ) { return world().type_nat(); }
const Def* Rewriter::rewrite_imm_Univ  (const Univ  *  ) { return world().univ(); }
const Def* Rewriter::rewrite_imm_App   (const App   * d) { return world().app   (rewrite(d->callee()), rewrite(d->arg()));                                }
const Def* Rewriter::rewrite_imm_Inj   (const Inj   * d) { return world().inj   (rewrite(d->type()),   rewrite(d->value()));                              }
const Def* Rewriter::rewrite_imm_Insert(const Insert* d) { return world().insert(rewrite(d->tuple()),  rewrite(d->index()), rewrite(d->value()));         }
const Def* Rewriter::rewrite_imm_Lit   (const Lit   * d) { return world().lit   (rewrite(d->type()),   d->get());                                         }
const Def* Rewriter::rewrite_imm_Merge (const Merge * d) { return world().merge (rewrite(d->type()),   rewrite(d->ops()));                                }
const Def* Rewriter::rewrite_imm_Proxy (const Proxy * d) { return world().proxy (rewrite(d->type()),   rewrite(d->ops()), d->pass(), d->tag());           }
const Def* Rewriter::rewrite_imm_Split (const Split * d) { return world().split (rewrite(d->type()),   rewrite(d->value()));                              }
const Def* Rewriter::rewrite_imm_Lam   (const Lam   * d) { return world().lam   (rewrite(d->type())->as<Pi>(), rewrite(d->filter()), rewrite(d->body())); }
const Def* Rewriter::rewrite_imm_Match (const Match * d) { return world().match (rewrite(d->ops()));                                                      }
const Def* Rewriter::rewrite_imm_Pi    (const Pi    * d) { return world().pi    (rewrite(d->dom()),    rewrite(d->codom()), d->is_implicit());            }
const Def* Rewriter::rewrite_imm_Tuple (const Tuple * d) { return world().tuple (rewrite(d->type()),   rewrite(d->ops()));                                }
const Def* Rewriter::rewrite_imm_Type  (const Type  * d) { return world().type  (rewrite(d->level()));                                                    }
const Def* Rewriter::rewrite_imm_UInc  (const UInc  * d) { return world().uinc  (rewrite(d->op()),     d->offset());                                      }
const Def* Rewriter::rewrite_imm_UMax  (const UMax  * d) { return world().umax  (rewrite(d->ops()));                                                      }
const Def* Rewriter::rewrite_imm_Uniq  (const Uniq  * d) { return world().uniq  (rewrite(d->op()));                                                       }
const Def* Rewriter::rewrite_imm_Var   (const Var   * d) { return world().var   (rewrite(d->type()),   rewrite(d->mut())->as_mut());                      }
const Def* Rewriter::rewrite_imm_Sigma (const Sigma * d) { return world().sigma (rewrite(d->ops()));                                                      }
const Def* Rewriter::rewrite_imm_Top   (const Top   * d) { return world().top   (rewrite(d->type()));                                                     }
const Def* Rewriter::rewrite_imm_Bot   (const Bot   * d) { return world().bot   (rewrite(d->type()));                                                     }
const Def* Rewriter::rewrite_imm_Meet  (const Meet  * d) { return world().meet  (rewrite(d->ops()));                                                      }
const Def* Rewriter::rewrite_imm_Join  (const Join  * d) { return world().join  (rewrite(d->ops()));                                                      }

const Def* Rewriter::rewrite_imm_Arr (const Arr * d) { return rewrite_imm_Seq(d); }
const Def* Rewriter::rewrite_imm_Pack(const Pack* d) { return rewrite_imm_Seq(d); }
const Def* Rewriter::rewrite_mut_Arr (      Arr * d) { return rewrite_mut_Seq(d); }
const Def* Rewriter::rewrite_mut_Pack(      Pack* d) { return rewrite_mut_Seq(d); }
// clang-format on

const Def* Rewriter::rewrite_imm_Axm(const Axm* a) {
    if (&a->world() != &world()) {
        auto type    = rewrite(a->type());
        auto new_axm = world().axm(a->normalizer(), a->curry(), a->trip(), type, a->plugin(), a->tag(), a->sub());
        return map(a, new_axm->set(a->dbg()));
    }
    return a;
}
const Def* Rewriter::rewrite_mut_Pi(Pi* pi) { return rewrite_mut_(pi); }
const Def* Rewriter::rewrite_mut_Lam(Lam* lam) { return rewrite_mut_(lam); }
const Def* Rewriter::rewrite_mut_Sigma(Sigma* sigma) { return rewrite_mut_(sigma); }
const Def* Rewriter::rewrite_mut_Global(Global* global) { return rewrite_mut_(global); }

const Def* Rewriter::rewrite_mut_Hole(Hole* hole) {
    auto [last, op] = hole->find();
    return op ? rewrite(op) : rewrite_mut_(last);
}

const Def* Rewriter::rewrite_mut_(Def* old_mut) {
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

const Def* Rewriter::rewrite_imm_Seq(const Seq* seq) {
    auto new_arity = rewrite(seq->arity());
    if (auto l = Lit::isa(new_arity); l && *l == 0) return world().prod(seq->is_intro(), {});
    return world().seq(seq->is_intro(), new_arity, rewrite(seq->body()));
}

const Def* Rewriter::rewrite_mut_Seq(Seq* seq) {
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
    return rewrite_mut_(seq->as_mut());
}

const Def* Rewriter::rewrite_imm_Extract(const Extract* ex) {
    auto new_index = rewrite(ex->index());
    if (auto index = Lit::isa(new_index)) {
        if (auto tuple = ex->tuple()->isa<Tuple>()) return map(ex, rewrite(tuple->op(*index)));
        if (auto pack = ex->tuple()->isa_imm<Pack>(); pack && pack->arity()->is_closed())
            return map(ex, rewrite(pack->body()));
    }

    auto new_tuple = rewrite(ex->tuple());
    return world().extract(new_tuple, new_index);
}

} // namespace mim
