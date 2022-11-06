#include "thorin/check.h"

#include "thorin/error.h"
#include "thorin/world.h"

namespace thorin {

const Def* infer_type_level(World& world, Defs defs) {
    // TODO deal with non-lit levels
    level_t level = 0;
    for (auto def : defs) {
        if (auto type = def->isa<Type>()) {
            level = std::max(level, as_lit(type->level()) + 1);
        } else if (auto type = def->type()->isa<Type>()) {
            level = std::max(level, as_lit(type->level()));
        } else {
            err(def->loc(), "'{}' used as a type but is in fact a term", def);
        }
    }
    return world.type(world.lit_univ(level));
}

bool Checker::equiv(const Def* d1, const Def* d2, const Def* dbg /*= {}*/) {
    if (d1 == d2) return true;
    if (!d1 || !d2 || !d1->is_set() || !d2->is_set()) return false;

    // normalize: always put smaller gid to the left
    if (d1->gid() > d2->gid()) std::swap(d1, d2);

    if (auto [it, ins] = equiv_.emplace(std::pair(d1, d2), Equiv::Unknown); !ins) {
        switch (it->second) {
            case Equiv::Distinct: return false;
            case Equiv::Unknown:
            case Equiv::Equiv: return true;
            default: unreachable();
        }
    }

    bool res                  = equiv_internal(d1, d2, dbg);
    equiv_[std::pair(d1, d2)] = res ? Equiv::Equiv : Equiv::Distinct;
    return res;
}

bool Checker::equiv_internal(const Def* d1, const Def* d2, const Def* dbg) {
    if (!equiv(d1->type(), d2->type(), dbg)) return false;
    if (d1->isa<Top>() || d2->isa<Top>()) return equiv(d1->type(), d2->type(), dbg);

    if (auto n1 = d1->isa_nom()) {
        if (auto n2 = d2->isa_nom()) vars_.emplace_back(n1, n2);
    }

    if (d1->isa<Sigma, Arr>()) {
        if (!equiv(d1->arity(), d2->arity(), dbg)) return false;

        if (auto a = isa_lit(d1->arity())) {
            for (size_t i = 0; i != a; ++i) {
                if (!equiv(d1->proj(*a, i), d2->proj(*a, i), dbg)) return false;
            }
            return true;
        }
    }

    if (d1->node() != d2->node() || d1->flags() != d2->flags() || d1->num_ops() != d2->num_ops()) return false;

    if (auto var = d1->isa<Var>()) { // vars are equal if they appeared under the same binder
        for (auto [n1, n2] : vars_) {
            if (var->nom() == n1) return d2->as<Var>()->nom() == n2;
        }
        return false;
    }

    return std::ranges::equal(d1->ops(), d2->ops(), [this, dbg](auto op1, auto op2) { return equiv(op1, op2, dbg); });
}

bool Checker::assignable(const Def* type, const Def* val, const Def* dbg /*= {}*/) {
    if (type == val->type()) return true;

    if (auto sigma = type->isa<Sigma>()) {
        if (!equiv(type->arity(), val->type()->arity(), dbg)) return false;

        auto red = sigma->reduce(val);
        for (size_t i = 0, a = red.size(); i != a; ++i) {
            if (!assignable(red[i], val->proj(a, i, dbg), dbg)) return false;
        }

        return true;
    } else if (auto arr = type->isa<Arr>()) {
        if (!equiv(type->arity(), val->type()->arity(), dbg)) return false;

        if (auto a = isa_lit(arr->arity())) {
            for (size_t i = 0; i != *a; ++i) {
                if (!assignable(arr->proj(*a, i), val->proj(*a, i, dbg), dbg)) return false;
            }

            return true;
        }
    } else if (auto vel = val->isa<Vel>()) {
        if (assignable(type, vel->value(), dbg)) return true;
    }

    return equiv(type, val->type(), dbg);
}

const Def* Checker::is_uniform(Defs defs, const Def* dbg) {
    assert(!defs.empty());
    auto first = defs.front();
    auto ops   = defs.skip_front();
    return std::ranges::all_of(ops, [this, first, dbg](auto op) { return equiv(first, op, dbg); }) ? first : nullptr;
}

} // namespace thorin
