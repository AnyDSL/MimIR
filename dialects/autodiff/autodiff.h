#pragma once

#include "thorin/world.h"

#include "dialects/autodiff/autogen.h"

namespace thorin::autodiff {

inline const Def* op_autodiff(const Def* fun, const Def* style) {
    World& world = fun->world();
    // We rely on the normalized thorin convention that all arguments in functions are grouped.
    // `cn[[args], cont:=cn[returns]]`
    return world.app(world.app(world.ax<autodiff>(), world.tuple({fun->type(), style})), fun);
}

inline const Def* op_zero(const Def* A) {
    World& world = A->world();
    A->dump();
    return world.app(world.ax<zero>(), A);
}

} // namespace thorin::autodiff
