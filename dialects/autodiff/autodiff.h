#pragma once

#include "thorin/world.h"

#include "dialects/autodiff/autogen.h"

namespace thorin::autodiff {

inline const Def* op_autodiff(const Def* fun) {
    World& world = fun->world();
    // We rely on the normalized thorin convention that all arguments in functions are grouped.
    // `cn[[args], cont:=cn[returns]]`
    return world.app(world.app(world.ax<ad>(), fun->type()), fun);
}

inline const Def* op_zero(const Def* A) {
    World& world = A->world();
    return world.app(world.ax<zero>(), A);
}

inline const Def* op_sum(const Def* T, DefArray defs) {
    // TODO: assert all are of type T
    auto& world = T->world();
    return world.raw_app(world.raw_app(world.ax<sum>(), {world.lit_nat(defs.size()), T}), world.tuple(defs));
}

inline const Def* op_add(const Def* a, const Def* b) {
    auto& world = a->world();
    return world.app(world.app(world.ax<add>(), a->type()), {a, b});
}

} // namespace thorin::autodiff
