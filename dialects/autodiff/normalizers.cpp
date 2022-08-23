#include <iostream>

#include "thorin/axiom.h"
#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/autodiff/autodiff.h"
namespace thorin::autodiff {

const Def* normalize_autodiff(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    // auto [mat, index, val] = arg->projs<3>();

    // do nothing (everything handled in the rewrite pass)
    // TODO: maybe directly handle operations

        printf("Normalize autodiff\n");
        return world.lit_int_width(32,42);
    // return world.raw_app(callee, arg, dbg);
}

const Def* normalize_autodiff_type(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
        printf("Normalize autodiff type\n");
        // return arg;
        return world.lit_int_width(32,42);
    // return autodiff_type(arg);
}

const Def* normalize_tangent_type(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return tangent_type(arg);
}

// TODO: zero of type Nat, Real, Int -> 0
const Def* normalize_zero(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    // auto [mat, index, val] = arg->projs<3>();

    // TODO:

    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_add(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    // auto [mat, index, val] = arg->projs<3>();

    // TODO:

    return world.raw_app(callee, arg, dbg);
}

THORIN_autodiff_NORMALIZER_IMPL

} // namespace thorin::autodiff
