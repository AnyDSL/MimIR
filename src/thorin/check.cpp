#include "thorin/check.h"

#include "thorin/rewrite.h"
#include "thorin/world.h"

namespace thorin {

namespace {

class InferRewriter : public Rewriter {
public:
    InferRewriter(World& world)
        : Rewriter(world) {}

    Ref rewrite(Ref old_def) override {
        if (Infer::should_eliminate(old_def)) return Rewriter::rewrite(old_def);
        return old_def;
    }
};

} // namespace

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

    // If we have an Infer as operand, try to get rid of it now.
    // TODO why does this not work?
    // if (res->isa_imm() && res->has_dep(Dep::Infer)) {
    if (res->isa<Tuple>() || res->isa<Type>()) {
        auto new_type = Ref::refer(res->type());
        bool update   = new_type != res->type();

        auto new_ops = DefVec(res->num_ops(), [res, &update](size_t i) {
            auto r = Ref::refer(res->op(i));
            update |= r != res->op(i);
            return r;
        });

        if (update) return res->rebuild(new_type, new_ops);
    }

    return res;
}

bool Infer::eliminate(Vector<Ref*> refs) {
    if (std::ranges::any_of(refs, [](auto pref) { return should_eliminate(*pref); })) {
        auto& world = (*refs.front())->world();
        InferRewriter rw(world);
        for (size_t i = 0, e = refs.size(); i != e; ++i) {
            auto ref = *refs[i];
            *refs[i] = ref->has_dep(Dep::Infer) ? rw.rewrite(ref) : ref;
        }
        return true;
    }
    return false;
}
/*
 * Check
 */

#ifdef THORIN_ENABLE_CHECKS
template<bool infer> bool Check::fail() {
    if (infer && world().flags().break_on_alpha_unequal) fe::breakpoint();
    return false;
}
#endif

template<bool infer> bool Check::alpha_(Ref r1, Ref r2) {
    auto d1 = *r1; // find
    auto d2 = *r2; // find

    if (!d1 && !d2) return true;
    if (!d1 || !d2) return fail<infer>();

    // It is only safe to check for pointer equality if there are no Vars involved.
    // Otherwise, we have to look more thoroughly.
    // Example: λx.x - λz.x
    if (!d1->has_dep(Dep::Var) && !d2->has_dep(Dep::Var) && d1 == d2) return true;
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

    auto i1 = d1->isa_mut<Infer>();
    auto i2 = d2->isa_mut<Infer>();

    if ((!i1 && !d1->is_set()) || (!i2 && !d2->is_set())) return fail<infer>();

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

    // normalize:
    if ((d1->isa<Lit>() && !d2->isa<Lit>())      // Lit to right
        || (!d1->isa<UMax>() && d2->isa<UMax>()) // UMax to left
        || (d1->gid() > d2->gid()))              // smaller gid to left
        std::swap(d1, d2);

    return alpha_internal<infer>(d1, d2);
}

template<bool infer> bool Check::alpha_internal(Ref d1, Ref d2) {
    if (!alpha_<infer>(d1->type(), d2->type())) return fail<infer>();
    if (d1->isa<Top>() || d2->isa<Top>()) return infer;
    if (!infer && (d1->isa_mut<Infer>() || d2->isa_mut<Infer>())) return fail<infer>();
    if (!alpha_<infer>(d1->arity(), d2->arity())) return fail<infer>();

    // vars are equal if they appeared under the same binder
    if (auto mut1 = d1->isa_mut()) assert_emplace(vars_, mut1, d2->isa_mut());
    if (auto mut2 = d2->isa_mut()) assert_emplace(vars_, mut2, d1->isa_mut());

    if (auto ts = d1->isa<Tuple, Sigma>()) {
        size_t a = ts->num_ops();
        for (size_t i = 0; i != a; ++i)
            if (!alpha_<infer>(ts->op(i), d2->proj(a, i))) return fail<infer>();
        return true;
    } else if (auto pa = d1->isa<Pack, Arr>()) {
        if (pa->node() == d2->node()) return alpha_<infer>(pa->ops().back(), d2->ops().back());
        if (auto a = pa->isa_lit_arity()) {
            for (size_t i = 0; i != *a; ++i)
                if (!alpha_<infer>(pa->proj(*a, i), d2->proj(*a, i))) return fail<infer>();
            return true;
        }
    } else if (auto umax = d1->isa<UMax>(); umax && umax->has_dep(Dep::Infer) && !d2->isa<UMax>()) {
        // .umax(a, ?) == x  =>  .umax(a, x)
        for (auto op : umax->ops())
            if (auto inf = op->isa_mut<Infer>(); inf && !inf->is_set()) inf->set(d2);
        d1 = umax->rebuild(umax->type(), umax->ops());
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return fail<infer>();

    if (auto var1 = d1->isa<Var>()) {
        auto var2 = d2->as<Var>();
        if (auto i = vars_.find(var1->mut()); i != vars_.end()) return i->second == var2->mut();
        if (auto i = vars_.find(var2->mut()); i != vars_.end()) return fail<infer>(); // var2 is bound
        // both var1 and var2 are free: OK, when they are the same or in infer mode
        return var1 == var2 || infer;
    }

    for (size_t i = 0, e = d1->num_ops(); i != e; ++i)
        if (!alpha_<infer>(d1->op(i), d2->op(i))) return fail<infer>();
    return true;
}

bool Check::assignable_(Ref type, Ref val) {
    auto val_ty = Ref::refer(val->type());
    if (type == val_ty) return true;

    if (auto infer = val->isa_mut<Infer>()) return alpha_<true>(type, infer->type());

    if (auto sigma = type->isa<Sigma>()) {
        if (!alpha_<true>(type->arity(), val_ty->arity())) return fail<true>();

        size_t a = sigma->num_ops();
        auto red = sigma->reduce(val);
        for (size_t i = 0; i != a; ++i)
            if (!assignable_(red[i], val->proj(a, i))) return fail<true>();
        return true;
    } else if (auto arr = type->isa<Arr>()) {
        if (!alpha_<true>(type->arity(), val_ty->arity())) return fail<true>();

        if (auto a = Lit::isa(arr->arity())) {
            for (size_t i = 0; i != *a; ++i)
                if (!assignable_(arr->proj(*a, i), val->proj(*a, i))) return fail<true>();
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
    for (size_t i = 1, e = defs.size(); i != e; ++i)
        if (!alpha<false>(first, defs[i])) return nullptr;
    return first;
}

/*
 * infer & check
 */

void Arr::check() {
    auto t = body()->unfold_type();
    if (!Check::alpha(t, type()))
        error(type()->loc(), "declared sort '{}' of array does not match inferred one '{}'", type(), t);
    if (t != type()) set_type(t);
}

Ref Sigma::infer(World& w, Defs ops) {
    if (ops.size() == 0) return w.type<1>();
    auto kinds = DefVec(ops.size(), [ops](size_t i) { return ops[i]->unfold_type(); });
    return w.umax<Sort::Kind>(kinds);
}

void Sigma::check() {
    auto t = infer(world(), ops());
    if (t != type()) {
        // TODO HACK
        if (Check::alpha(t, type())) set_type(t);
        world().WLOG("incorrect type '{}' for '{}'. Correct one would be: '{}'. I'll keep this one nevertheless due to "
                     "bugs in clos-conv",
                     type(), this, t);
    }
}

void Lam::check() {
    if (!Check::alpha(filter()->type(), world().type_bool())) {
        error(filter()->loc(), "filter '{}' of lambda is of type '{}' but must be of type '.Bool'", filter(),
              filter()->type());
    }
    if (!Check::assignable(codom(), body())) {
        throw Error()
            .error(body()->loc(), "body of function is not assignable to declared codomain")
            .note(body()->loc(), "body: '{}'", body())
            .note(body()->loc(), "type: '{}'", body()->type())
            .note(codom()->loc(), "codomain: '{}'", codom());
    }
}

Ref Pi::infer(Ref dom, Ref codom) {
    auto& w = dom->world();
    return w.umax<Sort::Kind>({dom->unfold_type(), codom->unfold_type()});
}

void Pi::check() {
    auto t = infer(dom(), codom());
    if (!Check::alpha(t, type()))
        error(type()->loc(), "declared sort '{}' of function type does not match inferred one '{}'", type(), t);
    if (t != type()) set_type(t);
}

#ifndef DOXYGEN
template bool Check::alpha_<true>(Ref, Ref);
template bool Check::alpha_<false>(Ref, Ref);
#endif

} // namespace thorin
