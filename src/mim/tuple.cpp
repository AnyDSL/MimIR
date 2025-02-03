#include "mim/tuple.h"

#include <cassert>

#include "mim/world.h"

// TODO this code needs to be rewritten

namespace mim {

namespace {
bool should_flatten(Ref def) {
    auto type = (def->is_term() ? def->type() : def);
    if (type->isa<Sigma>()) return true;
    if (auto arr = type->isa<Arr>()) {
        if (auto a = arr->isa_lit_arity(); a && *a > def->world().flags().scalarize_threshold) return false;
        return true;
    }
    return false;
}

bool mut_val_or_typ(Ref def) {
    auto typ = def->is_term() ? def->type() : def;
    return typ->isa_mut();
}

Ref unflatten(Defs defs, Ref type, size_t& j, bool flatten_muts) {
    if (!defs.empty() && defs[0]->type() == type) return defs[j++];
    if (auto a = type->isa_lit_arity();
        flatten_muts == mut_val_or_typ(type) && a && *a != 1 && a <= type->world().flags().scalarize_threshold) {
        auto& world = type->world();
        auto ops    = DefVec(*a, [&](size_t i) { return unflatten(defs, type->proj(*a, i), j, flatten_muts); });
        return world.tuple(type, ops);
    }

    return defs[j++];
}
} // namespace

Ref Pack::shape() const {
    if (auto arr = type()->isa<Arr>()) return arr->shape();
    if (type() == world().sigma()) return world().lit_nat_0();
    return world().lit_nat_1();
}

bool is_unit(Ref def) { return def->type() == def->world().sigma(); }

std::string tuple2str(Ref def) {
    if (def == nullptr) return {};

    auto array = def->projs(Lit::as(def->arity()), [](auto op) { return Lit::as(op); });
    return std::string(array.begin(), array.end());
}

size_t flatten(DefVec& ops, Ref def, bool flatten_muts) {
    if (auto a = def->isa_lit_arity(); a && *a != 1 && should_flatten(def) && flatten_muts == mut_val_or_typ(def)) {
        auto n = 0;
        for (size_t i = 0; i != *a; ++i) n += flatten(ops, def->proj(*a, i), flatten_muts);
        return n;
    } else {
        ops.emplace_back(def);
        return 1;
    }
}

Ref flatten(Ref def) {
    if (!should_flatten(def)) return def;
    DefVec ops;
    flatten(ops, def);
    return def->is_term() ? def->world().tuple(def->type(), ops) : def->world().sigma(ops);
}

Ref unflatten(Defs defs, Ref type, bool flatten_muts) {
    size_t j = 0;
    auto def = unflatten(defs, type, j, flatten_muts);
    assert(j == defs.size());
    return def;
}

Ref unflatten(Ref def, Ref type) { return unflatten(def->projs(Lit::as(def->arity())), type); }

DefVec merge(Ref def, Defs defs) {
    return DefVec(defs.size() + 1, [&](size_t i) { return i == 0 ? *def : defs[i - 1]; });
}

DefVec merge(Defs a, Defs b) {
    DefVec result(a.size() + b.size());
    auto [_, o] = std::ranges::copy(a, result.begin());
    std::ranges::copy(b, o);
    return result;
}

Ref merge_sigma(Ref def, Defs defs) {
    if (auto sigma = def->isa_imm<Sigma>()) return def->world().sigma(merge(sigma->ops(), defs));
    return def->world().sigma(merge(def, defs));
}

Ref merge_tuple(Ref def, Defs defs) {
    auto& w = def->world();
    if (auto sigma = def->type()->isa_imm<Sigma>()) {
        auto a     = sigma->num_ops();
        auto tuple = DefVec(a, [&](auto i) { return w.extract(def, a, i); });
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

} // namespace mim
