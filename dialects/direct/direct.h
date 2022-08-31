#pragma once

#include "thorin/world.h"

#include "dialects/direct/autogen.h"

namespace thorin::direct {

inline const Def* op_cps2ds(const Def* f) {
    auto& world = f->world();
    // TODO: assert continuation
    auto f_ty = f->type()->as<Pi>();
    auto T    = f_ty->dom(0);
    auto U    = f_ty->dom(1)->as<Pi>()->dom();
    // TODO: check if app can be used instead of raw_app
    return world.raw_app(world.raw_app(world.ax<direct::cps2ds>(), {T, U}), f);
}

} // namespace thorin::direct
