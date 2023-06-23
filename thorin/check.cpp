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
        if (Infer::should_eliminate(old_def)) return Rewriter::rewrite(old_def);
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
    // TODO don't re-update last infer

    // path compression: set all Infers along the chain to res
    for (auto infer = def->isa_mut<Infer>(); infer && infer->op(); infer = def->isa_mut<Infer>()) {
        def = infer->op();
        infer->reset(res);
    }

    assert((!res->isa<Infer>() || res != res->op(0)) && "an Infer shouldn't point to itself");
    return res;
}

Ref Infer::explode() {
    if (is_set()) return {};
    auto a = type()->isa_lit_arity();
    if (!a) return {};

    auto n      = *a;
    auto infers = DefArray(n);
    auto& w     = world();

    if (auto arr = type()->isa_imm<Arr>(); arr && n > world().flags().infer_arr_threshold) {
        auto pack = w.pack(arr->shape(), w.mut_infer(arr->body()));
        set(pack);
        return pack;
    }

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

bool Infer::eliminate(Array<Ref*> refs) {
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
template<Check::Mode mode> bool Check::fail() {
    if (mode == Mode::Relaxed && world().flags().break_on_alpha_unequal) breakpoint();
    return false;
}
#endif

template<Check::Mode mode> bool Check::alpha_(Ref r1, Ref r2) {
    auto d1 = *r1; // find
    auto d2 = *r2; // find
    assert(d1 && d2);

    // It is only safe to check for pointer equality if there are no Vars involved.
    // Otherwise, we have to look more thoroughly. Example: λx.x - λz.x
    if (!d1->has_dep(Dep::Var) && !d2->has_dep(Dep::Var) && d1 == d2) return true;
    auto mut1 = d1->isa_mut();
    auto mut2 = d2->isa_mut();
    if (mut1 && mut2 && mut1 == mut2) return true;

    if (mut1) {
        if (auto [i, ins] = done_.emplace(mut1, d2); !ins) return i->second == d2;
    }
    if (mut2) {
        if (auto [i, ins] = done_.emplace(mut2, d1); !ins) return i->second == d1;
    }

    auto i1 = d1->isa_mut<Infer>();
    auto i2 = d2->isa_mut<Infer>();

    if ((!i1 && !d1->is_set()) || (!i2 && !d2->is_set())) return fail<mode>();

    if (mode == Relaxed) {
        if (i1 && i2) {
            // union by rank: attach the lighter node to the heavier one
            if (i1->rank() < i2->rank())
                i1->set(i2);
            else if (i2->rank() < i1->rank())
                i2->set(i1);
            else { // pick i1 as new root
                i2->set(i1);
                ++i1->rank();
            }
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
    if (to_left<Extract>(d1, d2) || to_left<Tuple>(d1, d2) || to_left<Pack>(d1, d2) || to_left<Sigma>(d1, d2)
        || to_left<Arr>(d1, d2) || to_left<UMax>(d1, d2) || to_left<Type>(d1, d2) || to_left<Univ>(d1, d2))
        std::swap(d1, d2);

    return alpha_internal<mode>(d1, d2);
}

template<Check::Mode mode> bool Check::alpha_internal(Ref d1, Ref d2) {
    if (d1->isa<Univ>()) return d2->isa<Univ>();
    if (auto type1 = d1->isa<Type>()) {
        if (auto type2 = d2->isa<Type>()) return alpha_<mode>(type1->level(), type2->level());
        return fail<mode>();
    }

    if (!alpha_<mode>(d1->type(), d2->type())) return fail<mode>();
    if (d1->isa<Top>() || d2->isa<Top>()) return mode == Relaxed;
    if (mode != Relaxed && (d1->isa_mut<Infer>() || d2->isa_mut<Infer>())) return fail<mode>();
    if (!alpha_<mode>(d1->arity(), d2->arity())) return fail<mode>();

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
        d1 = umax->rebuild(world(), umax->type(), umax->ops());
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return fail<mode>();

    if (auto var1 = d1->isa<Var>()) {
        auto var2 = d2->as<Var>();
        if (auto i = vars_.find(var1->mut()); i != vars_.end()) return i->second == var2->mut();
        if (auto i = vars_.find(var2->mut()); i != vars_.end()) return fail<mode>(); // var2 is bound
        // both var1 and var2 are free: OK, when they in Mode::Relaxed
        return var1 == var2 || mode == Relaxed;
    }

    for (size_t i = 0, e = d1->num_ops(); i != e; ++i)
        if (!alpha_<mode>(d1->op(i), d2->op(i))) return fail<mode>();
    return true;
}

bool Check::assignable(Ref type, Ref value) {
    auto& world = type->world();
    auto check  = Check(world, true);
    if (check.assignable_(type, value)) return true;
    if (check.rerun() && Infer::eliminate(Array<Ref*>{&type, &value})) return Check(world).assignable_(type, value);
    return Check(world).fail<Relaxed>();
}

bool Check::assignable_(Ref type, Ref val) {
    auto val_ty = Ref::refer(val->unfold_type());
    if (type == val_ty) return true;

    if (auto infer = val->isa_mut<Infer>()) return alpha_<Relaxed>(type, infer->type());

    if (auto sigma = type->isa<Sigma>()) {
        if (!alpha_<Relaxed>(type->arity(), val_ty->arity())) return fail<Relaxed>();

        size_t a = sigma->num_ops();
        auto red = sigma->reduce(val);
        for (size_t i = 0; i != a; ++i)
            if (!assignable_(red[i], val->proj(a, i))) return fail<Relaxed>();
        return true;
    } else if (auto arr = type->isa<Arr>()) {
        if (!alpha_<Relaxed>(type->arity(), val_ty->arity())) return fail<Relaxed>();

        if (auto a = Lit::isa(arr->arity())) {
            for (size_t i = 0; i != *a; ++i)
                if (!assignable_(arr->proj(*a, i), val->proj(*a, i))) return fail<Relaxed>();
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

//------------------------------------------------------------------------------

#ifdef THORIN_ENABLE_CHECKS
Check2::Pair Check2::fail() {
    if (world().flags().break_on_alpha_unequal) breakpoint();
    return False;
}
#endif

Check2::Pair Check2::alpha_(Ref r1, Ref r2) {
    auto d1 = *r1; // find
    auto d2 = *r2; // find
    assert(d1 && d2);

    // It is only safe to check for pointer equality if there are no Vars involved.
    // Otherwise, we have to look more thoroughly. Example: λx.x - λz.x
    if (d1 == d2) {
        if ((!d1->has_dep(Dep::Var) && !d2->has_dep(Dep::Var)) || (d1->isa_mut() && d2->isa_mut())) return {d1, d2};
    }
    if (d1->isa<Univ>()) return fail();

    if (auto arr = d1->isa_mut<Arr>()) {
        auto [a1, a2] = alpha_(arr->arity(), d2->arity());
        if (auto a = Lit::isa(a1)) {
            if (Scope::is_free(arr, arr->body()))
                d1 = world().sigma(DefArray(*a, [&](size_t i) { return arr->reduce(world().lit_idx(*a, i)); }));
            else
                d1 = world().arr(arr->shape(), arr->body());
        }
    }

    if (auto arr = d2->isa_mut<Arr>()) {
        auto [a1, a2] = alpha_(arr->arity(), d2->arity());
        if (auto a = Lit::isa(a2)) {
            if (Scope::is_free(arr, arr->body()))
                d2 = world().sigma(DefArray(*a, [&](size_t i) { return arr->reduce(world().lit_idx(*a, i)); }));
            else
                d2 = world().arr(arr->shape(), arr->body());
        }
    }

    auto i1 = d1->isa_mut<Infer>();
    auto i2 = d2->isa_mut<Infer>();

    if (i1 && i2) {
        // union by rank: attach the lighter node to the heavier one
        if (i1->rank() < i2->rank()) {
            i1->set(i2);
            return {i2, i2};
        } else if (i2->rank() < i1->rank()) {
            i2->set(i1);
            return {i1, i1};
        } else { // pick i1 as new root
            i2->set(i1);
            ++i1->rank();
            return {i1, i1};
        }
    } else if (i1) {
        i1->set(d2);
        return {d2, d2};
    } else if (i2) {
        i2->set(d1);
        return {d1, d1};
    }

    if (!d1->is_set() || !d2->is_set()) return fail();

    /*auto [it2, ins2] =*/old2new_.emplace(d2, Pair{d2, d1});
    auto [it1, ins1] = old2new_.emplace(d1, Pair{d1, d2});
    // assert(d1 == d2 || !(ins1 ^ ins2));
    if (!ins1) return it1->second;

    auto res = alpha_internal(d1, d2);
    if (res == False) return fail();
    old2new_[d1] = res;
    old2new_[d2] = Pair{res.second, res.first};
    return res;
}

Check2::Pair Check2::alpha_internal(Ref d1, Ref d2) {
    if (auto type1 = d1->isa<Type>()) {
        if (auto type2 = d2->isa<Type>()) {
            auto [lvl1, lvl2] = alpha_(type1->level(), type2->level());
            if (lvl1) return {world().type(lvl1), world().type(lvl2)};
        }
        return fail();
    }

    auto [type1, type2] = alpha_(d1->type(), d2->type());
    if (!type1) return fail();

    // if (mode != Relaxed && (d1->isa_mut<Infer>() || d2->isa_mut<Infer>())) return false;

    if (auto mut1 = d1->isa_mut(); mut1 && d2->isa_imm()) {
        if (auto imm1 = mut1->immutabilize()) return alpha_(imm1, d2);
    } else if (auto mut2 = d2->isa_mut()) {
        if (auto imm2 = mut2->immutabilize()) return alpha_(d1, imm2);
    }

    if (auto p = alpha_symm(d1, d2)) return *p;
    if (auto p = alpha_symm(d2, d1)) return {p->second, p->first};

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return fail();

    size_t n = d1->num_ops();
    DefArray new_ops1(n), new_ops2(n);
    for (size_t i = 0; i != n; ++i) {
        auto p = alpha_(d1->op(i), d2->op(i));
        if (p == False) return fail();
        std::tie(new_ops1[i], new_ops2[i]) = p;
    }

    auto mut1 = d1->isa_mut();
    auto mut2 = d2->isa_mut();
    if (mut1 && mut2) {
        mut1->reset(new_ops1);
        mut2->reset(new_ops2);
    } else { // even if one is a mut, we can build an imm instead
        d1 = d1->rebuild(world(), type1, new_ops1);
        d2 = d2->rebuild(world(), type2, new_ops2);
    }

    return {d1, d2};
}

std::optional<Check2::Pair> Check2::alpha_symm(Ref d1, Ref d2) {
    if (d1->isa<Top>()) {
        return {Pair(d2, d2)};
    } else if (auto var1 = d1->isa<Var>()) {
        if (auto var2 = d2->isa<Var>()) {
            // vars are equal if they appeared under the same binder
            // TODO unify mutables?
            if (auto i = old2new_.find(var1->mut()); i != old2new_.end())
                return i->second.second == var2->mut() ? Pair{d1, d2} : fail();
            if (auto i = old2new_.find(var2->mut()); i != old2new_.end()) return fail(); // var2 is bound
            return {Pair(d1, d2)}; // both var1 and var2 are free: OK
        }
        return fail();
    } else if (auto umax = d1->isa<UMax>(); umax && umax->has_dep(Dep::Infer)) {
        if (auto l = d2->isa<Lit>()) {
            // .umax(a, ?) =α l  =>  .umax(a, l)
            size_t n = umax->num_ops();
            DefArray new_ops(n);
            for (size_t i = 0; i != n; ++i) {
                if (auto inf = umax->op(i)->isa_mut<Infer>(); inf && !inf->is_set()) {
                    inf->set(l);
                    new_ops[i] = l;
                } else {
                    new_ops[i] = umax->op(i);
                }
            }
            d1 = umax->rebuild(world(), umax->type(), umax->ops());
            return alpha_(d1, l);
        }
    } else if (auto sigma = d1->isa<Sigma>()) {
        if (auto arr = d2->isa<Arr>()) {
            if (alpha_(sigma->arity(), arr->arity()) == False) return fail();
            auto n = sigma->num_ops();
            DefArray new_ops1(n), new_ops2(n);
            for (size_t i = 0; i != n; ++i) {
                auto p = alpha_(sigma->op(i), arr->proj(n, i));
                if (p == False) return fail();
                std::tie(new_ops1[i], new_ops2[i]) = p;
            }

            auto s1 = world().sigma(new_ops1)->set(sigma->dbg());
            auto s2 = world().sigma(new_ops2)->set(arr->dbg());
            return {Pair(s1, s2)};
        }
    } else if (auto tuple = d1->isa<Tuple>()) {
        if (auto pack = d2->isa<Pack>()) {
            if (alpha_(tuple->arity(), pack->arity()) == False) return fail();
            auto n = tuple->num_ops();
            DefArray new_ops1(n), new_ops2(n);
            for (size_t i = 0; i != n; ++i) {
                auto p = alpha_(tuple->op(i), pack->proj(n, i));
                if (p == False) return fail();
                std::tie(new_ops1[i], new_ops2[i]) = p;
            }
            auto t1 = world().tuple(new_ops1)->set(tuple->dbg());
            auto t2 = world().tuple(new_ops2)->set(pack->dbg());
            return {Pair(t1, t2)};
        }
    } else if (auto extract = d1->isa<Extract>(); extract && !d2->isa<Extract>()) {
        if (auto idx = Lit::isa_idx(extract->index())) {
            auto [n, i] = *idx;
            if (auto elem = explode(extract->tuple(), n, i, d2)) {
                auto [elem1, elem2] = alpha_(elem, d2);
                // assert(elem1 == elem2);
                if (!elem1) return fail();
                return {Pair(elem1, elem1)};
            }
        } else {
            // TODO arr explode
            outln("TODO arr explode");
        }
    }

    return {};
}

Ref Check2::explode(Ref tuple, nat_t n, nat_t i, Ref value) {
    // outln("in: {} -- {}_{}", tuple, i, n);
    if (auto infer = tuple->isa_mut<Infer>()) tuple = infer->explode();

    if (auto tup = tuple->isa<Tuple>()) {
        assert(tup->num_ops() == n);
        // outln("out: {}", tup->op(i));
        return tup->op(i);
    } else if (auto extract = tuple->isa<Extract>(); extract && !value->isa<Extract>()) {
        if (auto idx = Lit::isa_idx(extract->index())) {
            auto [n, i] = *idx;
            auto res    = explode(extract->tuple(), n, i, value);
            // outln("out: {}", tuple->op(i));
            return res;
        }
    }

    return {};
}

Check2::Pair Check2::assignable_(Ref type, Ref val) {
    auto val_ty = Ref::refer(val->unfold_type());
    if (type == val_ty) return {type, val};

    if (auto infer = val->isa_mut<Infer>()) {
        if (auto [ty, _] = alpha_(type, infer->type()); ty) return {ty, val};
        return fail();
    }

    if (auto sigma = type->isa<Sigma>()) {
        if (alpha_(type->arity(), val_ty->arity()) == False) return fail();

        size_t a = sigma->num_ops();
        auto red = sigma->reduce(val);
        for (size_t i = 0; i != a; ++i)
            if (assignable_(red[i], val->proj(a, i)) == False) return fail();
        return {type, val};
    } else if (auto arr = type->isa<Arr>()) {
        if (alpha_(type->arity(), val_ty->arity()) == False) return fail();

        if (auto a = Lit::isa(arr->arity())) {
            for (size_t i = 0; i != *a; ++i)
                if (assignable_(arr->proj(*a, i), val->proj(*a, i)) == False) return fail();
            return {type, val};
        }
    } else if (auto vel = val->isa<Vel>()) {
        return assignable_(type, vel->value());
    }

    auto [t, _] = alpha_(type, val_ty);
    if (!t) return fail();
    return {t, val};
}

//------------------------------------------------------------------------------

/*
 * infer & check
 */

void Arr::check() {
    auto t = body()->unfold_type();
    if (Check2::alpha(t, type()) == Check2::False)
        error(type(), "declared sort '{}' of array does not match inferred one '{}'", type(), t);
}

void Sigma::check() {
    // TODO
}

void Lam::check() {
    if (Check2::alpha(filter()->type(), world().type_bool()) == Check2::False)
        error(filter(), "filter '{}' of lambda is of type '{}' but must be of type '.Bool'", filter(),
              filter()->type());
    if (Check2::assignable(codom(), body()) == Check2::False)
        error(body(), "body '{}' of lambda is of type \n'{}' but its codomain is of type \n'{}'", body(),
              body()->type(), codom());
}

Ref Pi::infer(Ref dom, Ref codom) {
    auto& w = dom->world();
    return w.umax<Sort::Kind>({dom->unfold_type(), codom->unfold_type()});
}

void Pi::check() {
    auto t = infer(dom(), codom());
    if (Check2::alpha(t, type()) == Check2::False)
        error(type(), "declared sort '{}' of function type does not match inferred one '{}'", type(), t);
}

void Infer::check() {
    // TODO We can't really check Infers while we are already checking stuff.
    // This is causing some recursive headaches.
    // if (!world().is_frozen() && !Check::assignable(type(), op()))
    // error(type(), "cannot assign '{}' of type '{}' to Infer of type '{}'", op(), op()->type(), type());
}

} // namespace thorin
