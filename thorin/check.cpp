#include "thorin/check.h"

#include "thorin/world.h"

namespace thorin {

/*
 * Infer
 */

const Def* Ref::refer(const Def* def) { return def ? Infer::find(def) : nullptr; }

const Def* Infer::find(const Def* def) {
    // find root
    auto res = def;
    for (auto infer = res->isa_mut<Infer>(); infer && infer->op(); infer = res->isa_mut<Infer>()) res = infer->op();

    // path compression: set all Infers along the chain to res
    for (auto infer = def->isa_mut<Infer>(); infer && infer->op(); infer = def->isa_mut<Infer>()) {
        def = infer->op();
        infer->reset(res);
    }

    assert((!res->isa<Infer>() || res != res->op(0)) && "an Infer shouldn't point to itself");

    // If we have an Infer as operand, try to get rid of it now.
    // TODO why does this not work?
    // if (res->isa_imm() && res->has_dep(Dep::Infer)) {
    if (res->isa<Tuple>() || res->isa<Type>()) {
        auto new_type = Ref::refer(res->type());
        bool update   = new_type != res->type();

        auto new_ops = DefArray(res->num_ops(), [res, &update](size_t i) {
            auto r = Ref::refer(res->op(i));
            update |= r != res->op(i);
            return r;
        });

        if (update) return res->rebuild(res->world(), new_type, new_ops);
    }

    return res;
}

/*
 * Check
 */

template<bool infer>
bool Check::alpha_(Ref r1, Ref r2) {
    auto d1 = *r1; // find
    auto d2 = *r2; // find

    if (d1 == d2) return true;
    if (!d1 || !d2) return false;

    auto i1 = d1->isa_mut<Infer>();
    auto i2 = d2->isa_mut<Infer>();

    if ((!i1 && !d1->is_set()) || (!i2 && !d2->is_set())) return false;

    if (infer) {
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

    // normalize: Lit to right; then sort by gid
    if ((d1->isa<Lit>() && !d2->isa<Lit>()) || (d1->gid() > d2->gid())) std::swap(d1, d2);

    auto mut1 = d1->isa_mut();
    auto mut2 = d2->isa_mut();

    if (mut1 && !done_.emplace(mut1).second) return true;
    if (mut2 && !done_.emplace(mut2).second) return true;
    return alpha_internal<infer>(d1, d2);
}

template<bool infer>
bool Check::alpha_internal(Ref d1, Ref d2) {
    if (!alpha_<infer>(d1->type(), d2->type())) return false;
    if (d1->isa<Top>() || d2->isa<Top>()) return infer;
    if (!alpha_<infer>(d1->arity(), d2->arity())) return false;
    if (!infer && (d1->isa_mut<Infer>() || d2->isa_mut<Infer>())) return false;

    struct Pop {
        ~Pop() {
            if (vars) vars->pop_back();
        }

        Vars* vars = nullptr;
    } pop;

    if (auto n1 = d1->isa_mut()) {
        if (auto n2 = d2->isa_mut()) {
            vars_.emplace_back(n1, n2);
            pop.vars = &vars_; // make sure vars_ is popped again
        }
    }

    if (auto sigma = d1->isa<Sigma>()) {
        size_t a = sigma->num_ops();
        for (size_t i = 0; i != a; ++i)
            if (!alpha_<infer>(d1->op(i), d2->proj(a, i))) return false;
        return true;
    } else if (auto arr1 = d1->isa<Arr>()) {
        // we've already checked arity above
        if (auto arr2 = d2->isa<Arr>()) return alpha_<infer>(arr1->body(), arr2->body());
        if (auto a = arr1->isa_lit_arity()) {
            for (size_t i = 0; i != *a; ++i)
                if (alpha_<infer>(d1->proj(*a, i), d2->proj(*a, i))) return false;
            return true;
        }
    } else if (auto umax = d1->isa<UMax>(); umax && umax->has_dep(Dep::Infer)) {
        // .umax(a, ?) == x  =>  .umax(a, x)
        for (auto op : umax->ops())
            if (auto inf = op->isa_mut<Infer>(); inf && !inf->is_set()) inf->set(d2);
        d1 = umax->rebuild(umax->world(), umax->type(), umax->ops());
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return false;

    if (auto var1 = d1->isa<Var>()) { // vars are equal if they appeared under the same binder
        auto var2   = d2->as<Var>();
        bool bound1 = false;
        bool bound2 = false;

        for (auto [n1, n2] : vars_) {
            if (var1->mut() == n1) {
                bound1 = true;
                return d2->as<Var>()->mut() == n2;
            }
            assert(var1->mut() != n2);
            if (var2->mut() == n1 || var2->mut() == n2) bound2 = true;
        }
        if (bound1 || bound2) return false;
        return infer; // both var1 and var2 are free: ok, when check%ing
    }

    for (size_t i = 0, e = d1->num_ops(); i != e; ++i)
        if (!alpha_<infer>(d1->op(i), d2->op(i))) return false;
    return true;
}

bool Check::assignable_(Ref type, Ref val) {
    auto val_ty = Ref::refer(val->type());
    if (type == val_ty) return true;

    if (auto infer = val->isa_mut<Infer>()) return alpha_<true>(type, infer->type());

    if (auto sigma = type->isa<Sigma>()) {
        if (!alpha_<true>(type->arity(), val_ty->arity())) return false;

        size_t a = sigma->num_ops();
        auto red = sigma->reduce(val);
        for (size_t i = 0; i != a; ++i)
            if (!assignable_(red[i], val->proj(a, i))) return false;
        return true;
    } else if (auto arr = type->isa<Arr>()) {
        if (!alpha_<true>(type->arity(), val_ty->arity())) return false;

        if (auto a = Lit::isa(arr->arity())) {
            for (size_t i = 0; i != *a; ++i)
                if (!assignable_(arr->proj(*a, i), val->proj(*a, i))) return false;
            return true;
        }
    } else if (auto vel = val->isa<Vel>()) {
        return assignable_(type, vel->value());
    }

    return alpha_<true>(type, val_ty);
}

Ref Check::is_uniform(Defs defs) {
    if (defs.empty()) return nullptr;
    auto first = defs.front();
    // outln("{}", first);
    for (size_t i = 1, e = defs.size(); i != e; ++i)
        if (!alpha<false>(first, defs[i])) return nullptr;
    // outln("{, } => {}", defs, first);
    return first;
}

/*
 * infer & check
 */

void Arr::check() {
    auto t = body()->unfold_type();
    if (!Check::alpha(t, type()))
        error(type(), "declared sort '{}' of array does not match inferred one '{}'", type(), t);
}

void Sigma::check() {
    // TODO
}

void Lam::check() {
    if (!Check::alpha(filter()->type(), world().type_bool()))
        error(filter(), "filter '{}' of lambda is of type '{}' but must be of type '.Bool'", filter(),
              filter()->type());
    if (!Check::alpha(body()->type(), codom()))
        error(body(), "body '{}' of lambda is of type \n'{}' but its codomain is of type \n'{}'", body(),
              body()->type(), codom());
}

Ref Pi::infer(Ref dom, Ref codom) {
    auto& w = dom->world();
    return w.umax<Sort::Kind>({dom->unfold_type(), codom->unfold_type()});
}

void Pi::check() {
    auto t = infer(dom(), codom());
    if (!Check::alpha(t, type()))
        error(type(), "declared sort '{}' of function type does not match inferred one '{}'", type(), t);
}

} // namespace thorin
