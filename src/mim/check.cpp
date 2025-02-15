#include "mim/check.h"

#include "mim/rewrite.h"
#include "mim/world.h"

#include "fe/assert.h"

namespace mim {

namespace {

class Zonker : public Rewriter {
public:
    Zonker(World& world)
        : Rewriter(world) {}

    Ref rewrite(Ref old_def) override { return (old_def->has_dep(Dep::Infer)) ? Rewriter::rewrite(old_def) : old_def; }
    Ref rewrite_mut(Def* mut) override { return mut; }
};

} // namespace

const Def* Def::zonk() const { return has_dep(Dep::Infer) ? *Zonker(world()).rewrite(this) : this; }

/*
 * Infer
 */

const Def* Ref::refer(const Def* def) { return def ? Infer::find(def) : nullptr; }

const Def* Infer::find(const Def* def) {
    // find root
    auto res = def;
    for (auto infer = res->isa_mut<Infer>(); infer && infer->op(); infer = res->isa_mut<Infer>()) res = infer->op();
    // TODO don't re-update last infer

    // path compression: set all Infers along the chain to res
    for (auto infer = def->isa_mut<Infer>(); infer && infer->op(); infer = def->isa_mut<Infer>()) {
        def = infer->op();
        infer->reset(res);
    }

    assert((!res->isa<Infer>() || res != res->op(0)) && "an Infer shouldn't point to itself");
    return res;
}

Ref Infer::tuplefy() {
    if (auto a = type()->isa_lit_arity(); a && !is_set()) {
        auto n      = *a;
        auto infers = DefVec(n);
        if (auto sigma = type()->isa_mut<Sigma>(); sigma && n >= 1 && sigma->has_var()) {
            auto var  = sigma->has_var();
            auto rw   = VarRewriter(var, this);
            infers[0] = world().mut_infer(sigma->op(0));
            for (size_t i = 1; i != n; ++i) {
                rw.map(sigma->var(n, i - 1), infers[i - 1]);
                infers[i] = world().mut_infer(rw.rewrite(sigma->op(i)));
            }
        } else {
            for (size_t i = 0; i != n; ++i) infers[i] = world().mut_infer(type()->proj(n, i));
        }

        auto tuple = world().tuple(infers);
        set(tuple);
        return tuple;
    }
    return this;
}

/*
 * Check
 */

#ifdef MIM_ENABLE_CHECKS
template<Checker::Mode mode> bool Checker::fail() {
    if (mode == Check && world().flags().break_on_alpha) fe::breakpoint();
    return false;
}

Ref Checker::fail() {
    if (world().flags().break_on_alpha) fe::breakpoint();
    return {};
}
#endif

template<Checker::Mode mode> bool Checker::alpha_(Ref r1, Ref r2) {
    auto d1 = *r1; // find
    auto d2 = *r2; // find

    if (!d1 && !d2) return true;
    if (!d1 || !d2) return fail<mode>();

    // It is only safe to check for pointer equality if there are no Vars involved.
    // Otherwise, we have to look more thoroughly.
    // Example: λx.x - λz.x
    if (!d1->has_dep(Dep::Var) && !d2->has_dep(Dep::Var) && d1 == d2) return true;

    auto i1 = d1->isa_mut<Infer>();
    auto i2 = d2->isa_mut<Infer>();

    if ((!i1 && !d1->is_set()) || (!i2 && !d2->is_set())) return fail<mode>();

    if (mode == Check) {
        if (i1 && i2) {
            // union by rank
            if (i1->rank() < i2->rank()) std::swap(i1, i2); // make sure i1 is heavier or equal
            i2->set(i1);                                    // make i1 new root
            if (i1->rank() == i2->rank()) ++i1->rank();
            return true;
        } else if (i1) {
            i1->set(d2);
            return true;
        } else if (i2) {
            i2->set(d1);
            return true;
        }
    }

    auto mut1 = d1->isa_mut();
    auto mut2 = d2->isa_mut();
    if (mut1 && mut2 && mut1 == mut2) return true;
    // Globals are HACKs and require additionaly HACKs:
    // Unless they are pointer equal (above) always consider them unequal.
    if (d1->isa<Global>() || d2->isa<Global>()) return false;

    if (mut1) {
        if (auto [i, ins] = done_.emplace(mut1, d2); !ins) return i->second == d2;
    }
    if (mut2) {
        if (auto [i, ins] = done_.emplace(mut2, d1); !ins) return i->second == d1;
    }

    // normalize:
    if ((d1->isa<Lit>() && !d2->isa<Lit>())             // Lit to right
        || (!d1->isa<UMax>() && d2->isa<UMax>())        // UMax to left
        || (!d1->isa<Extract>() && d2->isa<Extract>())) // Extract to left
        std::swap(d1, d2);

    return alpha_internal<mode>(d1, d2);
}

template<Checker::Mode mode> bool Checker::alpha_internal(Ref d1, Ref d2) {
    if (!alpha_<mode>(d1->type(), d2->type())) return fail<mode>();
    if (d1->isa<Top>() || d2->isa<Top>()) return mode == Check;
    if (mode == Opt && (d1->isa_mut<Infer>() || d2->isa_mut<Infer>())) return fail<mode>();
    if (!alpha_<mode>(d1->arity(), d2->arity())) return fail<mode>();

    // vars are equal if they appeared under the same binder
    if (auto mut1 = d1->isa_mut()) assert_emplace(vars_, mut1, d2->isa_mut());
    if (auto mut2 = d2->isa_mut()) assert_emplace(vars_, mut2, d1->isa_mut());

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
    } else if (auto umax = d1->isa<UMax>(); umax && umax->has_dep(Dep::Infer) && !d2->isa<UMax>()) {
        // .umax(a, ?) == x  =>  .umax(a, x)
        for (auto op : umax->ops())
            if (auto inf = op->isa_mut<Infer>(); inf && !inf->is_set()) inf->set(d2);
        d1 = umax->rebuild(umax->type(), umax->ops());
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return fail<mode>();

    if (auto var1 = d1->isa<Var>()) {
        auto var2 = d2->as<Var>();
        if (auto i = vars_.find(var1->mut()); i != vars_.end()) return i->second == var2->mut();
        if (auto i = vars_.find(var2->mut()); i != vars_.end()) return fail<mode>(); // var2 is bound
        // both var1 and var2 are free: OK, when they are the same or in Check mode
        return var1 == var2 || mode == Check;
    }

    for (size_t i = 0, e = d1->num_ops(); i != e; ++i)
        if (!alpha_<mode>(d1->op(i), d2->op(i))) return fail<mode>();
    return true;
}

Ref Checker::assignable_(Ref type, Ref val) {
    auto val_ty = Ref::refer(val->type());
    if (type == val_ty) return val;

    if (auto sigma = type->isa<Sigma>()) {
        if (!alpha_<Check>(type->arity(), val_ty->arity())) return fail();

        size_t a     = sigma->num_ops();
        auto red     = sigma->reduce(val);
        auto new_ops = DefVec(red.size());
        for (size_t i = 0; i != a; ++i) {
            auto new_val = assignable_(red[i], val->proj(a, i));
            if (new_val)
                new_ops[i] = new_val;
            else
                return fail();
        }
        return world().tuple(new_ops);
    } else if (auto arr = type->isa<Arr>()) {
        if (!alpha_<Check>(type->arity(), val_ty->arity())) return fail();

        // TODO ack sclarize threshold
        if (auto a = Lit::isa(arr->arity())) {
            auto new_ops = DefVec(*a);
            for (size_t i = 0; i != *a; ++i) {
                auto new_val = assignable_(arr->proj(*a, i), val->proj(*a, i));
                if (new_val)
                    new_ops[i] = new_val;
                else
                    return fail();
            }
            return world().tuple(new_ops);
        }
    } else if (auto vel = val->isa<Vel>()) {
        if (auto new_val = assignable_(type, vel->value())) return world().vel(type, new_val);
        return fail();
    } else if (auto uniq = val->type()->isa<Uniq>()) {
        if (auto new_val = assignable(type, uniq->inhabitant())) return new_val;
        return fail();
    }

    return alpha_<Check>(type, val_ty) ? val : fail();
}

Ref Checker::is_uniform(Defs defs) {
    if (defs.empty()) return nullptr;
    auto first = defs.front();
    for (size_t i = 1, e = defs.size(); i != e; ++i)
        if (!alpha<Opt>(first, defs[i])) return nullptr;
    return first;
}

/*
 * infer & check
 */

Ref Arr::check(size_t, Ref def) { return def; } // TODO

Ref Arr::check() {
    auto t = body()->unfold_type();
    if (!Checker::alpha<Checker::Check>(t, type()))
        error(type()->loc(), "declared sort '{}' of array does not match inferred one '{}'", type(), t);
    return t;
}

Ref Sigma::infer(World& w, Defs ops) {
    if (ops.size() == 0) return w.type<1>();
    auto kinds = DefVec(ops.size(), [ops](size_t i) { return ops[i]->unfold_type(); });
    return w.umax<Sort::Kind>(kinds);
}

Ref Sigma::check(size_t, Ref def) { return def; } // TODO

Ref Sigma::check() {
    auto t = infer(world(), ops());
    if (*t != *type()) {
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

Ref Lam::check(size_t i, Ref def) {
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

Ref Pi::infer(Ref dom, Ref codom) {
    auto& w = dom->world();
    return w.umax<Sort::Kind>({dom->unfold_type(), codom->unfold_type()});
}

Ref Pi::check(size_t, Ref def) { return def; }

Ref Pi::check() {
    auto t = infer(dom(), codom());
    if (!Checker::alpha<Checker::Check>(t, type()))
        error(type()->loc(), "declared sort '{}' of function type does not match inferred one '{}'", type(), t);
    return t;
}

#ifndef DOXYGEN
template bool Checker::alpha_<Checker::Check>(Ref, Ref);
template bool Checker::alpha_<Checker::Opt>(Ref, Ref);
#endif

} // namespace mim
