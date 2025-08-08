#include "mim/check.h"

#include <absl/container/fixed_array.h>
#include <fe/assert.h>

#include "mim/rewrite.h"
#include "mim/world.h"

namespace mim {

namespace {

static bool needs_zonk(const Def* def) {
    if (def->has_dep(Dep::Hole)) {
        for (auto mut : def->local_muts())
            if (auto hole = mut->isa<Hole>(); hole && hole->is_set()) return true;
    }

    return false;
}

class Zonker : public Rewriter {
public:
    Zonker(World& world, const Def* root)
        : Rewriter(world)
        , root_(root) {}

    const Def* rewrite(const Def* def) override {
        if (auto hole = def->isa_mut<Hole>()) {
            auto [last, op] = hole->find();
            return op ? rewrite(op) : last;
        }

        return def == root_ || needs_zonk(def) ? Rewriter::rewrite(def) : def;
    }

private:
    const Def* root_; // Always rewrite this one - coulde be a mutable!
};

} // namespace

const Def* Def::zonk() const { return needs_zonk(this) ? Zonker(world(), nullptr).rewrite(this) : this; }

const Def* Def::zonk_mut() {
    bool zonk = false;
    for (auto def : deps())
        if (needs_zonk(def)) {
            zonk = true;
            break;
        }

    if (zonk) {
        return Zonker(world(), this).rewrite(this);
        // TODO make this point to the new ops
    }

    if (auto imm = immutabilize()) return imm;
    return nullptr;
}

DefVec Def::zonk(Defs defs) {
    return DefVec(defs.size(), [defs](size_t i) { return defs[i]->zonk(); });
}

/*
 * Hole
 */

std::pair<Hole*, const Def*> Hole::find() {
    auto def  = Def::op(0);
    auto last = this;

    for (; def;) {
        if (auto h = def->isa_mut<Hole>()) {
            def  = h->op();
            last = h;
        } else {
            break;
        }
    }

    auto root = def ? def : last;

    // path compression
    for (auto h = this; h != last;) {
        auto next = h->op()->as_mut<Hole>();
        h->unset()->set(root);
        h = next;
    }

    return {last, def};
}

const Def* Hole::tuplefy(nat_t n) {
    if (is_set()) return this;

    auto& w    = world();
    auto holes = absl::FixedArray<const Def*>(n);
    if (auto sigma = type()->isa_mut<Sigma>(); sigma && n >= 1 && sigma->has_var()) {
        auto var = sigma->has_var();
        auto rw  = VarRewriter(var, this);
        holes[0] = w.mut_hole(sigma->op(0));
        for (size_t i = 1; i != n; ++i) {
            rw.map(sigma->var(n, i - 1), holes[i - 1]);
            holes[i] = w.mut_hole(rw.rewrite(sigma->op(i)));
        }
    } else {
        for (size_t i = 0; i != n; ++i) holes[i] = w.mut_hole(type()->proj(n, i));
    }

    auto tuple = w.tuple(holes);
    set(tuple);
    return tuple;
}

/*
 * Check
 */

#ifdef MIM_ENABLE_CHECKS
template<Checker::Mode mode> bool Checker::fail() {
    if (mode == Check && world().flags().break_on_alpha) fe::breakpoint();
    return false;
}

const Def* Checker::fail() {
    if (world().flags().break_on_alpha) fe::breakpoint();
    return {};
}
#endif

template<Checker::Mode mode> bool Checker::check(const Prod* prod, const Def* def) {
    size_t a = prod->num_ops();
    for (size_t i = 0; i != a; ++i)
        if (!alpha_<mode>(prod->op(i), def->proj(a, i))) return fail<mode>();
    return true;
}

template<Checker::Mode mode> bool Checker::check(const Seq* seq1, const Def* def) {
    if constexpr (mode == Mode::Check) {
        // alpha(«1; body», def) -> alpha(body, def);
        if (seq1->shape() == world().lit_nat_1() && !def->isa<Seq>()) {
            auto body = seq1->reduce(world().lit_idx(1, 0))->zonk(); // try to get rid of var inside of body
            if (!alpha_<mode>(body, def)) return fail<mode>();
            if (auto mut_seq = seq1->isa_mut<Seq>()) mut_seq->unset()->set(world().lit_nat_1(), body->zonk());
            return true;
        }
    }

    if (auto mut_seq = seq1->isa_mut<Seq>()) {
        if (auto imm_seq = def->isa_imm<Seq>()) {
            // Try to get rid of mut_seq's var: it may fly around in its body and vanish after reduction
            // as holes might have been filled in the meantime.
            auto mut_body = mut_seq->reduce(world().top(world().type_idx(mut_seq->shape())));
            if (!alpha_<Check>(mut_body, imm_seq->body())) return fail<Check>();

            auto mut_shape = mut_seq->shape();
            mut_seq->unset()->set(mut_shape, mut_body->zonk());
            return true;
        }
    }

    if (auto seq2 = def->isa<Seq>()) return alpha_<mode>(seq1->body(), seq2->body());

    return fail<mode>();
}

bool Checker::check(const UMax* umax, const Def* def) {
    for (auto op : umax->ops())
        if (!alpha<Check>(op, def)) return fail<Check>();
    return true;
}

template<Checker::Mode mode> bool Checker::alpha_(const Def* d1_, const Def* d2_) {
    auto ds        = std::array<const Def*, 2>{d1_->zonk(), d2_->zonk()};
    auto& [d1, d2] = ds;

    // It is only safe to check for pointer equality if there are no Vars involved.
    // Otherwise, we have to look more thoroughly.
    // Example: λx.x - λz.x
    if (!d1->has_dep(Dep::Var) && !d2->has_dep(Dep::Var) && d1 == d2) return true;

    auto h1 = d1->isa_mut<Hole>();
    auto h2 = d2->isa_mut<Hole>();

    if constexpr (mode == Check) {
        if (h1) return h1->set(d2), true;
        if (h2) return h2->set(d1), true;
    } else if (h1 || h2) // mode == Test and h1 or h2 is an unresolved Hole
        return fail<Test>();

    if (!d1->is_set() || !d2->is_set()) return fail<mode>();

    auto muts          = std::array<Def*, 2>{d1->isa_mut(), d2->isa_mut()};
    auto& [mut1, mut2] = muts;

    if (mut1 && mut2 && mut1 == mut2) return true;

    // Globals are HACKs and require additionaly HACKs:
    // Unless they are pointer equal (above) always consider them unequal.
    if (d1->isa<Global>() || d2->isa<Global>()) return false;

    bool redo = false;
    for (size_t i = 0; i != 2; ++i) {
        if (auto& mut = muts[i]) {
            size_t other = (i + 1) % 2;

            if (auto zonked = mut->zonk_mut())
                mut = nullptr, ds[i] = zonked, redo = true;
            else if (auto [i, ins] = binders_.emplace(mut, ds[other]); !ins)
                return i->second == ds[other];
        }
    }

    return redo ? alpha<mode>(d1, d2) : alpha_internal<mode>(d1, d2);
}

template<Checker::Mode mode> bool Checker::alpha_internal(const Def* d1, const Def* d2) {
    if (d1->type() && d2->type() && !alpha_<mode>(d1->type(), d2->type())) return fail<mode>();
    if (d1->isa<Top>() || d2->isa<Top>()) return mode == Check;
    if (!alpha_<mode>(d1->arity(), d2->arity())) return fail<mode>();

    d1 = d1->zonk();
    d2 = d2->zonk();

    // clang-format off
    if (auto prod = d1->isa<Prod>()) return check<mode>(prod, d2);
    if (auto prod = d2->isa<Prod>()) return check<mode>(prod, d1);
    if (auto seq  = d1->isa<Seq >()) return check<mode>(seq , d2);
    if (auto seq  = d2->isa<Seq >()) return check<mode>(seq , d1);
    // clang-format on

    if constexpr (mode == Check) {
        if (auto umax = d1->isa<UMax>(); umax && !d2->isa<UMax>()) return check(umax, d2);
        if (auto umax = d2->isa<UMax>(); umax && !d1->isa<UMax>()) return check(umax, d1);
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return fail<mode>();

    if (auto var1 = d1->isa<Var>()) {
        auto var2 = d2->as<Var>();
        if (auto i = binders_.find(var1->mut()); i != binders_.end()) return i->second == var2->mut();
        if (auto i = binders_.find(var2->mut()); i != binders_.end()) return fail<mode>(); // var2 is bound
        // both var1 and var2 are free: OK, when they are the same or in Check mode
        return var1 == var2 || mode == Check;
    }

    for (size_t i = 0, e = d1->num_ops(); i != e; ++i)
        if (!alpha_<mode>(d1->op(i), d2->op(i))) return fail<mode>();
    return true;
}

const Def* Checker::assignable_(const Def* type, const Def* val) {
    auto val_ty = val->type()->zonk();
    if (type == val_ty) return val;

    auto& w = world();
    if (auto sigma = type->isa<Sigma>()) {
        if (!alpha_<Check>(type->arity(), val_ty->arity())) return fail();

        size_t a     = sigma->num_ops();
        auto red     = sigma->reduce(val);
        auto new_ops = absl::FixedArray<const Def*>(red.size());
        for (size_t i = 0; i != a; ++i) {
            auto new_val = assignable_(red[i], val->proj(a, i));
            if (new_val)
                new_ops[i] = new_val;
            else
                return fail();
        }
        return w.tuple(new_ops);
    } else if (auto arr = type->isa<Arr>()) {
        if (!alpha_<Check>(type->arity(), val_ty->arity())) return fail();

        if (auto val_a = val_ty->isa<Arr>()) {
            if (alpha_<Check>(arr->body(), val_a->body())) return val;
            // TODO optimize this case here to not run into the expensive loop below
        }

        // TODO ack sclarize threshold
        if (auto a = arr->isa_lit_arity()) {
            auto new_ops = absl::FixedArray<const Def*>(*a);
            for (size_t i = 0; i != *a; ++i) {
                auto new_val = assignable_(arr->proj(*a, i), val->proj(*a, i));
                if (new_val)
                    new_ops[i] = new_val;
                else
                    return fail();
            }
            return w.tuple(new_ops);
        }
    } else if (auto inj = val->isa<Inj>()) {
        if (auto new_val = assignable_(type, inj->value())) return w.inj(type, new_val);
        return fail();
    } else if (auto uniq = val->type()->isa<Uniq>()) {
        if (auto new_val = assignable(type, uniq->inhabitant())) return new_val;
        return fail();
    }

    return alpha_<Check>(type, val_ty) ? val : fail();
}

const Def* Checker::is_uniform(Defs defs) {
    if (defs.empty()) return nullptr;
    auto first = defs.front();
    for (size_t i = 1, e = defs.size(); i != e; ++i)
        if (!alpha<Test>(first, defs[i])) return nullptr;
    return first;
}

/*
 * infer & check
 */

const Def* Arr::check(size_t, const Def* def) { return def; } // TODO

const Def* Arr::check() {
    auto t = body()->unfold_type();
    if (!Checker::alpha<Checker::Check>(t, type()))
        error(type()->loc(), "declared sort '{}' of array does not match inferred one '{}'", type(), t);
    return t;
}

const Def* Tuple::infer(World& world, Defs ops) {
    auto elems = absl::FixedArray<const Def*>(ops.size());
    for (size_t i = 0, e = ops.size(); i != e; ++i) elems[i] = ops[i]->unfold_type();
    return world.sigma(elems);
}

const Def* Sigma::infer(World& w, Defs ops) {
    auto elems = absl::FixedArray<const Def*>(ops.size());
    for (size_t i = 0, e = ops.size(); i != e; ++i) elems[i] = ops[i]->unfold_type();
    return w.umax<UMax::Kind>(elems);
}

const Def* Sigma::check(size_t, const Def* def) { return def; } // TODO

const Def* Sigma::check() {
    auto t = infer(world(), ops());
    if (t != type()) {
        // TODO HACK
        if (Checker::alpha<Checker::Check>(t, type()))
            return t;
        else {
            world().WLOG(
                "incorrect type '{}' for '{}'. Correct one would be: '{}'. I'll keep this one nevertheless due to "
                "bugs in clos-conv",
                type(), this, t);
            return type();
        }
    }
    return t;
}

const Def* Pi::infer(const Def* dom, const Def* codom) {
    auto& w = dom->world();
    return w.umax<UMax::Kind>({dom->unfold_type(), codom->unfold_type()});
}

const Def* Pi::check(size_t, const Def* def) { return def; }

const Def* Pi::check() {
    auto t = infer(dom(), codom());
    if (!Checker::alpha<Checker::Check>(t, type()))
        error(type()->loc(), "declared sort '{}' of function type does not match inferred one '{}'", type(), t);
    return t;
}

const Def* Lam::check(size_t i, const Def* def) {
    if (i == 0) {
        if (auto filter = Checker::assignable(world().type_bool(), def)) return filter;
        throw Error().error(filter()->loc(), "filter '{}' of lambda is of type '{}' but must be of type 'Bool'",
                            filter(), filter()->type());
    } else if (i == 1) {
        if (auto body = Checker::assignable(codom(), def)) return body;
        throw Error()
            .error(def->loc(), "body of function is not assignable to declared codomain")
            .note(def->loc(), "body: '{}'", def)
            .note(def->loc(), "type: '{}'", def->type())
            .note(codom()->loc(), "codomain: '{}'", codom());
    }
    fe::unreachable();
}

#ifndef DOXYGEN
template bool Checker::alpha_<Checker::Check>(const Def*, const Def*);
template bool Checker::alpha_<Checker::Test>(const Def*, const Def*);
#endif

} // namespace mim
