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
    Zonker(World& world)
        : Rewriter(world) {}

    const Def* rewrite(const Def* def) override {
        def = Hole::find(def);
        return needs_zonk(def) ? Rewriter::rewrite(def) : def;
    }
};

} // namespace

const Def* Def::zonk() const {
    auto def = Hole::find(this);
    return needs_zonk(def) ? Zonker(world()).rewrite(def) : def;
}

/*
 * Hole
 */

const Def* Hole::find(const Def* def) {
    auto hole1 = def->isa_mut<Hole>();
    if (!hole1 || !hole1->op()) return def; // early exit if def isn't a Hole or unset;

    // find root
    auto res = def;
    for (auto hole = hole1; hole && hole->op(); res = hole->op(), hole = res->isa_mut<Hole>()) {}

    // path compression
    for (auto hole = hole1;;) {
        auto next = hole->op();
        if (next == res) break;

        hole->unset()->set(res);
        hole = next->isa_mut<Hole>();
    }

    return res;
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

template<Checker::Mode mode> bool Checker::alpha_(const Def* d1_, const Def* d2_) {
    auto ds        = std::array<const Def*, 2>{Hole::find(d1_), Hole::find(d2_)};
    auto& [d1, d2] = ds;

    if (!d1 && !d2) return true;
    if (!d1 || !d2) return fail<mode>();

    // It is only safe to check for pointer equality if there are no Vars involved.
    // Otherwise, we have to look more thoroughly.
    // Example: λx.x - λz.x
    if (!d1->has_dep(Dep::Var) && !d2->has_dep(Dep::Var) && d1 == d2) return true;

    auto h1 = d1->isa_mut<Hole>();
    auto h2 = d2->isa_mut<Hole>();

    if ((!h1 && !d1->is_set()) || (!h2 && !d2->is_set())) return fail<mode>();

    if (mode == Check) {
        if (h1)
            return h1->set(d2), true;
        else if (h2)
            return h2->set(d1), true;
    }

    auto muts          = std::array<Def*, 2>{d1->isa_mut(), d2->isa_mut()};
    auto& [mut1, mut2] = muts;

    if (mut1 && mut2 && mut1 == mut2) return true;

    // Globals are HACKs and require additionaly HACKs:
    // Unless they are pointer equal (above) always consider them unequal.
    if (d1->isa<Global>() || d2->isa<Global>()) return false;

    for (size_t i = 0; i != 2; ++i) {
        auto& mut = muts[i];
        if (!mut || !mut->is_set()) continue;

        bool zonk = false;
        for (auto def : mut->deps())
            if (needs_zonk(def)) {
                zonk = true;
                break;
            }

        if (zonk) {
            auto defs = DefVec(mut->ops().begin(), mut->ops().end());
            mut->unset();
            for (size_t i = 0, e = mut->num_ops(); i != e; ++i) mut->set(i, defs[i]->zonk());
            // mut->type() will be automatically zonked after last op has been set
        }

        size_t other = (i + 1) % 2;
        if (auto imm = mut->immutabilize())
            mut = nullptr, ds[i] = imm;
        else if (auto [i, ins] = binders_.emplace(mut, ds[other]); !ins)
            return i->second == ds[other];
    }

    return alpha_internal<mode>(d1, d2);
}

template<Checker::Mode mode> bool Checker::alpha_internal(const Def* d1, const Def* d2) {
    if (d1->type() && d2->type() && !alpha_<mode>(d1->type(), d2->type())) return fail<mode>();
    if (d1->isa<Top>() || d2->isa<Top>()) return mode == Check;
    if (mode == Test && (d1->isa_mut<Hole>() || d2->isa_mut<Hole>())) return fail<mode>();
    if (!alpha_<mode>(d1->arity(), d2->arity())) return fail<mode>();

    if (auto ts = d1->isa<Tuple, Sigma>()) {
        size_t a = ts->num_ops();
        for (size_t i = 0; i != a; ++i)
            if (!alpha_<mode>(ts->op(i), d2->proj(a, i))) return fail<mode>();
        return true;
    } else if (auto pa = d1->isa<Pack, Arr>()) {
        if (pa->node() == d2->node()) return alpha_<mode>(pa->ops().back(), d2->ops().back());
        if (auto a = pa->isa_lit_arity()) {
            for (size_t i = 0; i != *a; ++i)
                if (!alpha_<mode>(pa->proj(*a, i), d2->proj(*a, i))) return fail<mode>();
            return true;
        }
    } else if (auto umax = d1->isa<UMax>(); umax && umax->has_dep(Dep::Hole) && !d2->isa<UMax>()) {
        // .umax(a, ?) == x  =>  .umax(a, x)
        for (auto op : umax->ops())
            if (auto inf = op->isa_mut<Hole>(); inf && !inf->is_set()) inf->set(d2);
        d1 = umax->rebuild(umax->type(), umax->ops());
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
    auto val_ty = Hole::find(val->type());
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

        // TODO ack sclarize threshold
        if (auto a = Lit::isa(arr->arity())) {
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
    return w.umax<Sort::Kind>(elems);
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
    return w.umax<Sort::Kind>({dom->unfold_type(), codom->unfold_type()});
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
