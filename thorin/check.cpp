#include "thorin/check.h"

#include <algorithm>

#include "thorin/rewrite.h"
#include "thorin/world.h"

namespace thorin {

namespace {

class InferRewriter : public Rewriter {
public:
    InferRewriter(World& world)
        : Rewriter(world) {}

    Ref rewrite(Ref old_def) override {
        if (!old_def || old_def->isa_mut()) return old_def;
        if (old_def->has_dep(Dep::Infer)) return Rewriter::rewrite(old_def);
        return old_def;
    }
};

template<class T> bool to_left(const Def* d1, const Def* d2) { return !d1->isa<T>() && d2->isa<T>(); }

} // namespace

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

Ref Infer::explode() {
    if (is_set()) return {};
    auto a = type()->isa_lit_arity();
    if (!a) return {};

    auto n      = *a;
    auto infers = DefArray(n);
    auto& w     = world();

    if (auto sigma = type()->isa_mut<Sigma>(); sigma && n >= 1 && sigma->var()) {
        Scope scope(sigma);
        ScopeRewriter rw(scope);
        infers[0] = w.mut_infer(sigma->op(0));
        for (size_t i = 1; i != n; ++i) {
            rw.map(sigma->var(n, i - 1), infers[i - 1]);
            infers[i] = w.mut_infer(rw.rewrite(sigma->op(i)));
        }
    } else {
        for (size_t i = 0; i != n; ++i) infers[i] = w.mut_infer(type()->proj(n, i));
    }

    auto tuple = w.tuple(infers);
    set(tuple);
    return tuple;
}

bool Infer::eliminate(RefArray refs) {
    if (std::ranges::any_of(refs, [](auto def) { return def->has_dep(Dep::Infer); })) {
        auto& world = refs.front()->world();
        InferRewriter rw(world);
        for (size_t i = 0, e = refs.size(); i != e; ++i)
            refs[i] = refs[i]->has_dep(Dep::Infer) ? rw.rewrite(refs[i]) : refs[i];
        return true;
    }
    return false;
}

/*
 * Check
 */

template<Check::Mode mode> bool Check::alpha_(Ref r1, Ref r2) {
    auto d1 = *r1; // find
    auto d2 = *r2; // find

    if (!d1 && !d2) return true;
    if (!d1 || !d2) return false;

    // It is only safe to check for pointer equality if there are no Vars involved.
    // Otherwise, we have to look more thoroughly. Example: λx.x - λz.x
    if (!d1->has_dep(Dep::Var) && !d2->has_dep(Dep::Var) && d1 == d2) return true;
    auto mut1 = d1->isa_mut();
    auto mut2 = d2->isa_mut();
    if (mut1 && mut2 && mut1 == mut2) return true;
    if (mut1 && !done_.emplace(mut1).second) return true;
    if (mut2 && !done_.emplace(mut2).second) return true;

    auto i1 = d1->isa_mut<Infer>();
    auto i2 = d2->isa_mut<Infer>();

    if ((!i1 && !d1->is_set()) || (!i2 && !d2->is_set())) return false;

    if (mode == Relaxed) {
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

    return alpha_internal<mode>(d1, d2);
}

template<Check::Mode mode> bool Check::alpha_internal(Ref d1, Ref d2) {
    if (!alpha_<mode>(d1->type(), d2->type())) return false;
    if (d1->isa<Top>() || d2->isa<Top>()) return mode == Relaxed;
    if (mode != Relaxed && (d1->isa_mut<Infer>() || d2->isa_mut<Infer>())) return false;
    if (!alpha_<mode>(d1->arity(), d2->arity())) return false;

    // normalize:
    if (to_left<Extract>(d1, d2) || to_left<Tuple>(d1, d2) || to_left<Pack>(d1, d2) || to_left<Sigma>(d1, d2)
        || to_left<Arr>(d1, d2) || to_left<UMax>(d1, d2))
        std::swap(d1, d2);

    // vars are equal if they appeared under the same binder
    if (auto mut1 = d1->isa_mut()) assert_emplace(vars_, mut1, d2->isa_mut());
    if (auto mut2 = d2->isa_mut()) assert_emplace(vars_, mut2, d1->isa_mut());

    // TODO more than one level
    if (auto extract = d1->isa<Extract>(); extract && !d2->isa<Extract>()) {
        if (auto infer = extract->tuple()->isa_mut<Infer>()) infer->explode();
    }

    if (auto ts = d1->isa<Tuple, Sigma>()) {
        size_t a = ts->num_ops();
        for (size_t i = 0; i != a; ++i)
            if (!alpha_<mode>(ts->op(i), d2->proj(a, i))) return false;
        return true;
    } else if (auto pa = d1->isa<Pack, Arr>()) {
        if (pa->node() == d2->node()) return alpha_<mode>(pa->ops().back(), d2->ops().back());
        if (auto a = pa->isa_lit_arity()) {
            for (size_t i = 0; i != *a; ++i)
                if (!alpha_<mode>(pa->proj(*a, i), d2->proj(*a, i))) return false;
            return true;
        }
    } else if (auto umax = d1->isa<UMax>(); umax && umax->has_dep(Dep::Infer) && !d2->isa<UMax>()) {
        // .umax(a, ?) == x  =>  .umax(a, x)
        for (auto op : umax->ops())
            if (auto inf = op->isa_mut<Infer>(); inf && !inf->is_set()) inf->set(d2);
        d1 = umax->rebuild(umax->world(), umax->type(), umax->ops());
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return false;

    if (auto var1 = d1->isa<Var>()) {
        auto var2 = d2->as<Var>();
        if (auto i = vars_.find(var1->mut()); i != vars_.end()) return i->second == var2->mut();
        if (auto i = vars_.find(var2->mut()); i != vars_.end()) return false; // var2 is bound
        // both var1 and var2 are free: OK, when they in Mode::Relaxed
        return var1 == var2 || mode == Relaxed;
    }

    for (size_t i = 0, e = d1->num_ops(); i != e; ++i)
        if (!alpha_<mode>(d1->op(i), d2->op(i))) return false;
    return true;
}

bool Check::assignable(Ref type, Ref value) {
    auto check = Check();
    if (check.assignable_(type, value)) return true;

    if (!check.rerun() && Infer::eliminate(RefArray({type, value}))) return Check(true).assignable_(type, value);

    return false;
}

bool Check::assignable_(Ref type, Ref val) {
    auto val_ty = Ref::refer(val->type());
    if (type == val_ty) return true;

    if (auto infer = val->isa_mut<Infer>()) return alpha_<Relaxed>(type, infer->type());

    if (auto sigma = type->isa<Sigma>()) {
        if (!alpha_<Relaxed>(type->arity(), val_ty->arity())) return false;

        size_t a = sigma->num_ops();
        auto red = sigma->reduce(val);
        for (size_t i = 0; i != a; ++i)
            if (!assignable_(red[i], val->proj(a, i))) return false;
        return true;
    } else if (auto arr = type->isa<Arr>()) {
        if (!alpha_<Relaxed>(type->arity(), val_ty->arity())) return false;

        if (auto a = Lit::isa(arr->arity())) {
            for (size_t i = 0; i != *a; ++i)
                if (!assignable_(arr->proj(*a, i), val->proj(*a, i))) return false;
            return true;
        }
    } else if (auto vel = val->isa<Vel>()) {
        return assignable_(type, vel->value());
    }

    return alpha_<Relaxed>(type, val_ty);
}

Ref Check::is_uniform(Defs defs) {
    if (defs.empty()) return nullptr;
    auto first = defs.front();
    for (size_t i = 1, e = defs.size(); i != e; ++i)
        if (!alpha<Strict>(first, defs[i])) return nullptr;
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
    if (!Check::assignable(codom(), body()))
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
