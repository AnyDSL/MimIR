#include "thorin/check.h"

#include "thorin/error.h"
#include "thorin/world.h"

namespace thorin {

bool Checker::equiv(Refer d1, Refer d2, Refer dbg, bool opt) {
    if (d1 == d2) return true;
    if (!d1 || !d2) return false;

    // normalize: Infer to the left; smaller gid to the left (with this priority)
    auto i1 = d1->isa_nom<Infer>();
    auto i2 = d2->isa_nom<Infer>();
    if ((!i1 && !d1->is_set()) || (!i2 && !d2->is_set())) return false;

    if (i1 && i2) {
        if (i1->is_set() && i2->is_set()) return equiv(i1->op(), i2->op(), dbg, opt);
        if (i1->is_set() && !i2->is_set()) {
            i2->set(i1->op());
            return true;
        }
        if (!i1->is_set() && i2->is_set()) {
            i1->set(i2->op());
            return true;
        }
        return opt;
    } else if (!i1 && i2) {
        std::swap(d1, d2);
    } else if (i1 && !i2) {
        // do nothing
    } else if (!i1 && !i2) {
        if (d1->gid() > d2->gid()) std::swap(d1, d2);
    }

    if (auto infer = d1->isa_nom<Infer>()) {
        if (infer->is_set()) {
            d1 = infer->op();
        } else {
            if (opt) {
                infer->set(d2);
                return true;
            } else {
                return false;
            }
        }
    }

    if (auto [it, ins] = equiv_.emplace(std::pair(d1, d2), Equiv::Unknown); !ins) {
        switch (it->second) {
            case Equiv::Distinct: return false;
            case Equiv::Unknown:
            case Equiv::Equiv: return true;
            default: unreachable();
        }
    }

    bool res = equiv_internal(d1, d2, dbg, opt);
    auto i = equiv_.find(std::pair(d1, d2));
    if (opt)
        i->second = res ? Equiv::Equiv : Equiv::Distinct;
    else
        equiv_.erase(i);
    return res;
}

bool Checker::equiv_internal(Refer d1, Refer d2, Refer dbg, bool opt) {
    if (!equiv(d1->type(), d2->type(), dbg, opt)) return false;
    if (d1->isa<Top>() || d2->isa<Top>()) return equiv(d1->type(), d2->type(), dbg, opt);

    if (auto n1 = d1->isa_nom()) {
        if (auto n2 = d2->isa_nom()) vars_.emplace_back(n1, n2);
    }

    if (d1->isa<Sigma, Arr>()) {
        if (!equiv(d1->arity(), d2->arity(), dbg, opt)) return false;

        if (auto a = isa_lit(d1->arity())) {
            for (size_t i = 0; i != a; ++i) {
                if (!equiv(d1->proj(*a, i), d2->proj(*a, i), dbg, opt)) return false;
            }
            return true;
        }
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return false;

    if (auto var = d1->isa<Var>()) { // vars are equal if they appeared under the same binder
        for (auto [n1, n2] : vars_) {
            if (var->nom() == n1) return d2->as<Var>()->nom() == n2;
        }
        return opt; // Var is free
    }

    return std::ranges::equal(d1->ops(), d2->ops(), [&](auto op1, auto op2) { return equiv(op1, op2, dbg, opt); });
}

bool Checker::assignable(Refer type, Refer val, Refer dbg /*= {}*/) {
    auto val_ty = refer(val->type());
    if (type == val_ty) return true;

    if (auto sigma = type->isa<Sigma>()) {
        auto infer = val->isa_nom<Infer>();
        if (!infer && !equiv(type->arity(), val_ty->arity(), dbg)) return false;

        size_t a = sigma->num_ops();
        auto red = sigma->reduce(val);

        if (infer && !infer->is_set()) {
            Array<const Def*> infer_ops(a, [&](size_t i) { return world().nom_infer(red[i], dbg); });
            infer->set(world().tuple(infer_ops, dbg));
            if (auto t = infer->type()->isa_nom<Infer>(); t && !t->is_set()) t->set(sigma);
        }

        for (size_t i = 0; i != a; ++i) {
            if (!assignable(red[i], val->proj(a, i, dbg), dbg)) return false;
        }

        return true;
    } else if (auto arr = type->isa<Arr>()) {
        if (!equiv(type->arity(), val_ty->arity(), dbg)) return false;

        if (auto a = isa_lit(arr->arity())) {
            for (size_t i = 0; i != *a; ++i) {
                if (!assignable(arr->proj(*a, i), val->proj(*a, i, dbg), dbg)) return false;
            }

            return true;
        }
    } else if (auto vel = val->isa<Vel>()) {
        if (assignable(type, vel->value(), dbg)) return true;
    }

    return equiv(type, val_ty, dbg);
}

const Def* Checker::is_uniform(Defs defs, Refer dbg) {
    assert(!defs.empty());
    auto first = defs.front();
    auto ops   = defs.skip_front();
    return std::ranges::all_of(ops, [&](auto op) { return equiv(first, op, dbg, false); }) ? first : nullptr;
}

/*
 * Def::check
 */

void Arr::check() {
    auto t = body()->unfold_type();
    if (auto infer = type()->isa_nom<Infer>()) {
        assert(infer->op() == nullptr);
        infer->set(t);
        set_type(t);
    }
}

void Sigma::check() {
    // TODO
}

void Lam::check() {
    auto& w = world();
    if (w.err()) {
        if (!w.checker().equiv(filter()->type(), w.type_bool(), filter()->dbg()))
            w.err()->err(filter()->loc(), "filter of lambda is of type '{}' but must be of type '.Bool'",
                         filter()->type());
        if (!w.checker().equiv(body()->type(), codom(), body()->dbg()))
            w.err()->err(body()->loc(), "body of lambda is of type '{}' but its codomain is of type '{}'",
                         body()->type(), codom());
    }
}

} // namespace thorin
