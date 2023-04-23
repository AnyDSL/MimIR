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
        infer->unset()->set(res);
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
 * Checker
 */

bool Checker::equiv(Ref r1, Ref r2) {
    auto d1 = *r1; // find
    auto d2 = *r2; // find

    if (d1 == d2) return true;
    if (!d1 || !d2) return false;

    auto i1 = d1->isa_mut<Infer>();
    auto i2 = d2->isa_mut<Infer>();

    if ((!i1 && !d1->is_set()) || (!i2 && !d2->is_set())) return false;

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

    assert(!i1 && !i2);
    if (d1->gid() > d2->gid()) std::swap(d1, d2); // normalize

    if (auto [it, ins] = equiv_.emplace(std::pair(d1, d2), Equiv::Unknown); !ins) {
        switch (it->second) {
            case Equiv::Distinct: return false;
            case Equiv::Unknown:
            case Equiv::Equiv: return true;
            default: unreachable();
        }
    }

    bool res                  = equiv_internal(d1, d2);
    equiv_[std::pair(d1, d2)] = res ? Equiv::Equiv : Equiv::Distinct;
    return res;
}

bool Checker::equiv_internal(Ref d1, Ref d2) {
    if (!equiv(d1->type(), d2->type())) return false;
    if (d1->isa<Top>() || d2->isa<Top>()) return equiv(d1->type(), d2->type());

    if (auto n1 = d1->isa_mut()) {
        if (auto n2 = d2->isa_mut()) vars_.emplace_back(n1, n2);
    }

    if (d1->isa<Sigma, Arr>()) {
        if (!equiv(d1->arity(), d2->arity())) return false;

        if (auto a = d1->isa_lit_arity()) {
            for (size_t i = 0; i != a; ++i)
                if (!equiv(d1->proj(*a, i), d2->proj(*a, i))) return false;
            return true;
        }
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return false;

    if (auto var = d1->isa<Var>()) { // vars are equal if they appeared under the same binder
        for (auto [n1, n2] : vars_)
            if (var->mut() == n1) return d2->as<Var>()->mut() == n2;
        // TODO what if Var is free?
        return false;
    }

    return std::ranges::equal(d1->ops(), d2->ops(), [&](auto op1, auto op2) { return equiv(op1, op2); });
}

bool Checker::assignable(Ref type, Ref val) {
    auto val_ty = Ref::refer(val->type());
    if (type == val_ty) return true;

    if (auto infer = val->isa_mut<Infer>()) return equiv(type, infer->type());

    if (auto sigma = type->isa<Sigma>()) {
        if (!equiv(type->arity(), val_ty->arity())) return false;

        size_t a = sigma->num_ops();
        auto red = sigma->reduce(val);

        for (size_t i = 0; i != a; ++i)
            if (!assignable(red[i], val->proj(a, i))) return false;

        return true;
    } else if (auto arr = type->isa<Arr>()) {
        if (!equiv(type->arity(), val_ty->arity())) return false;

        if (auto a = Lit::isa(arr->arity())) {
            for (size_t i = 0; i != *a; ++i)
                if (!assignable(arr->proj(*a, i), val->proj(*a, i))) return false;

            return true;
        }
    } else if (auto vel = val->isa<Vel>()) {
        if (assignable(type, vel->value())) return true;
    }

    return equiv(type, val_ty);
}

const Def* Checker::is_uniform(Defs defs) {
    assert(!defs.empty());
    auto first = defs.front();
    auto ops   = defs.skip_front();
    return std::ranges::all_of(ops, [&](auto op) { return equiv(first, op); }) ? first : nullptr;
}

/*
 * infer
 */

const Def* Pi::infer(const Def* dom, const Def* codom) {
    auto& w = dom->world();
    return w.umax<Sort::Kind>({dom->unfold_type(), codom->unfold_type()});
}

/*
 * check
 */

void Arr::check() {
    auto& w = world();
    auto t  = body()->unfold_type();

    if (!w.checker().equiv(t, type()))
        error(type(), "declared sort '{}' of array does not match inferred one '{}'", type(), t);
}

void Sigma::check() {
    // TODO
}

void Lam::check() {
    auto& w = world();
    return; // TODO
    if (!w.checker().equiv(filter()->type(), w.type_bool()))
        error(filter(), "filter '{}' of lambda is of type '{}' but must be of type '.Bool'", filter(),
              filter()->type());
    if (!w.checker().equiv(body()->type(), codom()))
        error(body(), "body '{}' of lambda is of type '{}' but its codomain is of type '{}'", body(), body()->type(),
              codom());
}

void Pi::check() {
    auto& w = world();
    auto t  = infer(dom(), codom());

    if (!w.checker().equiv(t, type()))
        error(type(), "declared sort '{}' of function type does not match inferred one '{}'", type(), t);
}

} // namespace thorin
