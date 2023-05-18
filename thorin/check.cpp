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
 * Checker
 */

Equiv Checker::equiv_(Ref r1, Ref r2) {
    auto d1 = *r1; // find
    auto d2 = *r2; // find

    if (d1 == d2) return Equiv::Yes;
    if (!d1 || !d2) return Equiv::No;

    auto i1 = d1->isa_mut<Infer>();
    auto i2 = d2->isa_mut<Infer>();

    if ((!i1 && !d1->is_set()) || (!i2 && !d2->is_set())) return Equiv::No;

    if (i1 && i2) {
        // union by rank
        if (i1->rank() < i2->rank()) std::swap(i1, i2); // make sure i1 is heavier or equal
        i2->set(i1);                                    // make i1 new root
        if (i1->rank() == i2->rank()) ++i1->rank();
        return Equiv::Yes;
    } else if (i1) {
        i1->set(d2);
        return Equiv::Yes;
    } else if (i2) {
        i2->set(d1);
        return Equiv::Yes;
    }

    assert(!i1 && !i2);
    // normalize: Lit to right; then sort by gid
    if ((d1->isa<Lit>() && !d2->isa<Lit>()) || (d1->gid() > d2->gid())) std::swap(d1, d2);

    auto mut1 = d1->isa_mut();
    auto mut2 = d2->isa_mut();

    if (mut1 && !done_.emplace(mut1).second) return Equiv::Yes;
    if (mut2 && !done_.emplace(mut2).second) return Equiv::Yes;
    return equiv_internal(d1, d2);
}

Equiv Checker::equiv_internal(Ref d1, Ref d2) {
    auto res = equiv_(d1->type(), d2->type());
    if (d1->isa<Top>() || d2->isa<Top>()) return meet(res, equiv_(d1->type(), d2->type()));
    if (res == Equiv::No) return Equiv::No;

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

    if (d1->isa<Sigma, Arr>()) {
        res = meet(res, equiv_(d1->arity(), d2->arity()));
        if (res == Equiv::No) return Equiv::No;

        if (auto a = d1->isa_lit_arity()) {
            for (size_t i = 0; i != a && res != Equiv::No; ++i)
                res = meet(res, equiv_(d1->proj(*a, i), d2->proj(*a, i)));
            return res;
        }
    }

    if (auto umax = d1->isa<UMax>(); umax && umax->has_dep(Dep::Infer)) {
        if (auto l = d2->isa<Lit>()) {
            for (auto op : umax->ops())
                if (auto infer = op->isa_mut<Infer>(); infer && !infer->is_set()) infer->set(l);
        }
        d1 = umax->rebuild(world(), umax->type(), umax->ops());
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return Equiv::No;

    if (auto var1 = d1->isa<Var>()) { // vars are equal if they appeared under the same binder
        auto var2   = d2->as<Var>();
        bool bound1 = false ;
        bool bound2 = false;

        for (auto [n1, n2] : vars_) {
            if (var1->mut() == n1) {
                bound1 = true;
                if (d2->as<Var>()->mut() != n2)
                    return Equiv::No;
                else
                    return res;
            }
            assert(var1->mut() != n2);
            if (var2->mut() == n1 || var2->mut() == n2) bound2 = true;
        }
        if (bound1 || bound2) return Equiv::No;
        return Equiv::FV; // both var1 and var2 are free
    }

    for (size_t i = 0, e = d1->num_ops(); i != e && res != Equiv::No; ++i)
        res = meet(res, equiv_(d1->op(i), d2->op(i)));
    return res;
}

Equiv Checker::assignable_(Ref type, Ref val) {
    auto val_ty = Ref::refer(val->type());
    if (type == val_ty) return Equiv::Yes;

    if (auto infer = val->isa_mut<Infer>()) return equiv_(type, infer->type());

    if (auto sigma = type->isa<Sigma>()) {
        if (equiv_(type->arity(), val_ty->arity()) == Equiv::No) return Equiv::No;

        size_t a = sigma->num_ops();
        auto red = sigma->reduce(val);
        auto res = Equiv::Yes;

        for (size_t i = 0; i != a && res != Equiv::No; ++i)
            res = meet(res, assignable_(red[i], val->proj(a, i)));
        return res;
    } else if (auto arr = type->isa<Arr>()) {
        if (equiv_(type->arity(), val_ty->arity()) == Equiv::No) return Equiv::No;

        if (auto a = Lit::isa(arr->arity())) {
            auto res = Equiv::Yes;
            for (size_t i = 0; i != *a && res != Equiv::No; ++i)
                res = meet(res, assignable_(arr->proj(*a, i), val->proj(*a, i)));
            return res;
        }
    } else if (auto vel = val->isa<Vel>()) {
        return assignable_(type, vel->value());
    }

    return equiv_(type, val_ty);
}

const Def* Checker::is_uniform(Defs defs) {
    assert(!defs.empty());
    auto first = defs.front();
    auto ops   = defs.skip_front();
    return std::ranges::all_of(ops, [&](auto op) { return equiv_(first, op) == Equiv::Yes; }) ? first : nullptr;
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
    auto t  = body()->unfold_type();
    if (!equiv(t, type()))
        error(type(), "declared sort '{}' of array does not match inferred one '{}'", type(), t);
}

void Sigma::check() {
    // TODO
}

void Lam::check() {
    if (!equiv(filter()->type(), world().type_bool()))
        error(filter(), "filter '{}' of lambda is of type '{}' but must be of type '.Bool'", filter(),
              filter()->type());
    if (!equiv(body()->type(), codom()))
        error(body(), "body '{}' of lambda is of type \n'{}' but its codomain is of type \n'{}'", body(),
              body()->type(), codom());
}

void Pi::check() {
    auto t  = infer(dom(), codom());
    if (!equiv(t, type()))
        error(type(), "declared sort '{}' of function type does not match inferred one '{}'", type(), t);
}

} // namespace thorin
