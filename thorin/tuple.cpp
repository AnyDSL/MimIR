#include "thorin/tuple.h"

#include <cassert>

#include "thorin/rewrite.h"
#include "thorin/world.h"

// TODO this code needs to be rewritten

namespace thorin {

static bool should_flatten(const Def* def) {
    return (def->sort() == Sort::Term ? def->type() : def)->isa<Sigma, Arr>();
}

static bool nom_val_or_typ(const Def* def) {
    auto typ = (def->sort() == Sort::Term) ? def->type() : def;
    return typ->isa_nom();
}

size_t flatten(DefVec& ops, const Def* def, bool flatten_noms) {
    if (auto a = isa_lit<nat_t>(def->arity());
        a && *a != 1 && should_flatten(def) && flatten_noms == nom_val_or_typ(def)) {
        auto n = 0;
        for (size_t i = 0; i != *a; ++i) n += flatten(ops, def->proj(*a, i), flatten_noms);
        return n;
    } else {
        ops.emplace_back(def);
        return 1;
    }
}

const Def* flatten(const Def* def) {
    if (!should_flatten(def)) return def;
    DefVec ops;
    flatten(ops, def);
    return def->sort() == Sort::Term ? def->world().tuple(def->type(), ops) : def->world().sigma(ops);
}

static const Def* unflatten(Defs defs, const Def* type, size_t& j, bool flatten_noms) {
    if (!defs.empty() && defs[0]->type() == type) return defs[j++];
    if (auto a = isa_lit<nat_t>(type->arity()); flatten_noms == nom_val_or_typ(type) && a && *a != 1) {
        auto& world = type->world();
        DefArray ops(*a, [&](size_t i) { return unflatten(defs, type->proj(*a, i), j, flatten_noms); });
        return world.tuple(type, ops);
    }

    return defs[j++];
}

const Def* unflatten(Defs defs, const Def* type, bool flatten_noms) {
    size_t j = 0;
    auto def = unflatten(defs, type, j, flatten_noms);
    assert(j == defs.size());
    return def;
}

const Def* unflatten(const Def* def, const Def* type) { return unflatten(def->projs(as_lit(def->arity())), type); }

bool is_unit(const Def* def) { return def->type() == def->world().sigma(); }

DefArray merge(const Def* def, Defs defs) {
    return DefArray(defs.size() + 1, [&](auto i) { return i == 0 ? def : defs[i - 1]; });
}

DefArray merge(Defs a, Defs b) {
    DefArray result(a.size() + b.size());
    auto [_, o] = std::ranges::copy(a, result.begin());
    std::ranges::copy(b, o);
    return result;
}

const Def* merge_sigma(const Def* def, Defs defs) {
    if (auto sigma = def->isa<Sigma>(); sigma && !sigma->isa_nom())
        return def->world().sigma(merge(sigma->ops(), defs));
    return def->world().sigma(merge(def, defs));
}

const Def* merge_tuple(const Def* def, Defs defs) {
    auto& w = def->world();
    if (auto sigma = def->type()->isa<Sigma>(); sigma && !sigma->isa_nom()) {
        auto a = sigma->num_ops();
        DefArray tuple(a, [&](auto i) { return w.extract(def, a, i); });
        return w.tuple(merge(tuple, defs));
    }

    return def->world().tuple(merge(def, defs));
}

std::string tuple2str(const Def* def) {
    if (def == nullptr) return {};

    auto array = def->projs(as_lit(def->arity()), as_lit<nat_t>);
    return std::string(array.begin(), array.end());
}

/*
 * reduce
 */

const Def* Arr::reduce(const Def* arg) const {
    if (auto nom = isa_nom<Arr>()) return rewrite(nom, arg, 1);
    return body();
}

const Def* Pack::reduce(const Def* arg) const {
    if (auto nom = isa_nom<Pack>()) return rewrite(nom, arg, 0);
    return body();
}

/*
 * Sym/Tuple
 */

Ref sym2def(World& world, Sym s) {
    DefVec ops;
    for (auto c : s) ops.emplace_back(world.lit_nat(c));
    return world.tuple(ops);
}

Sym def2sym(Ref tuple) {
    std::string s;
    for (size_t i = 0, n = tuple->num_projs(); i != n; ++i)
        if (auto lit = isa_lit(tuple->op(i)); lit && 1 <= *lit && *lit <= 127)
            s += (char)*lit;
        else
            return {};
    return tuple->world().sym(std::move(s));
}

} // namespace thorin
