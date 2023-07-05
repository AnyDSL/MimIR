#include "thorin/tuple.h"

#include <cassert>

#include "thorin/rewrite.h"
#include "thorin/world.h"

// TODO this code needs to be rewritten

namespace thorin {

namespace {
bool should_flatten(nat_t threshold, const Def* def) {
    auto type = (def->is_term() ? def->type() : def);
    if (type->isa<Sigma>()) return true;
    if (auto arr = type->isa<Arr>()) {
        if (auto a = arr->isa_lit_arity(); a && *a > threshold) return false;
        return true;
    }
    return false;
}

bool mut_val_or_typ(const Def* def) {
    auto typ = def->is_term() ? def->type() : def;
    return typ->isa_mut();
}

const Def* unflatten(nat_t threshold, Defs defs, const Def* type, size_t& j, bool flatten_muts) {
    if (!defs.empty() && defs[0]->type() == type) return defs[j++];
    if (auto a = type->isa_lit_arity(); flatten_muts == mut_val_or_typ(type) && a && *a != 1 && a <= threshold) {
        auto& world = type->world();
        DefArray ops(*a, [&](size_t i) { return unflatten(threshold, defs, type->proj(*a, i), j, flatten_muts); });
        return world.tuple(type, ops);
    }

    return defs[j++];
}
} // namespace

const Def* Pack::shape() const {
    if (auto arr = type()->isa<Arr>()) return arr->shape();
    if (type() == world().sigma()) return world().lit_nat_0();
    return world().lit_nat_1();
}

bool is_unit(const Def* def) { return def->type() == def->world().sigma(); }

std::string tuple2str(const Def* def) {
    if (def == nullptr) return {};

    auto array = def->projs(Lit::as(def->arity()), [](auto op) { return Lit::as(op); });
    return std::string(array.begin(), array.end());
}

size_t flatten(nat_t threshold, DefVec& ops, const Def* def, bool flatten_muts) {
    if (auto a = def->isa_lit_arity();
        a && *a != 1 && should_flatten(threshold, def) && flatten_muts == mut_val_or_typ(def)) {
        auto n = 0;
        for (size_t i = 0; i != *a; ++i) n += flatten(threshold, ops, def->proj(*a, i), flatten_muts);
        return n;
    } else {
        ops.emplace_back(def);
        return 1;
    }
}

const Def* flatten(nat_t threshold, const Def* def) {
    if (!should_flatten(threshold, def)) return def;
    DefVec ops;
    flatten(threshold, ops, def);
    return def->is_term() ? def->world().tuple(def->type(), ops) : def->world().sigma(ops);
}

const Def* unflatten(nat_t threshold, Defs defs, const Def* type, bool flatten_muts) {
    size_t j = 0;
    auto def = unflatten(threshold, defs, type, j, flatten_muts);
    assert(j == defs.size());
    return def;
}

const Def* unflatten(nat_t threshold, const Def* def, const Def* type) {
    return unflatten(threshold, def->projs(Lit::as(def->arity())), type);
}

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
    if (auto sigma = def->isa_imm<Sigma>()) return def->world().sigma(merge(sigma->ops(), defs));
    return def->world().sigma(merge(def, defs));
}

const Def* merge_tuple(const Def* def, Defs defs) {
    auto& w = def->world();
    if (auto sigma = def->type()->isa_imm<Sigma>()) {
        auto a = sigma->num_ops();
        DefArray tuple(a, [&](auto i) { return w.extract(def, a, i); });
        return w.tuple(merge(tuple, defs));
    }

    return def->world().tuple(merge(def, defs));
}

Ref tuple_of_types(Ref t) {
    auto& world = t->world();
    if (auto sigma = t->isa<Sigma>()) return world.tuple(sigma->ops());
    if (auto arr = t->isa<Arr>()) return world.pack(arr->shape(), arr->body());
    return t;
}

} // namespace thorin
