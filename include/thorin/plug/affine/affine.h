#pragma once

#include "thorin/world.h"

#include "thorin/plug/affine/autogen.h"

namespace thorin::plug::affine {

/// @name %%affine.For
///@{
/// Returns the affine_for axiom applied with \a params.
/// See documentation for %affine.For axiom in @ref affine.
inline const Def* fn_for(World& w, Defs params) {
    return w.app(w.annex<affine::For>(), {w.nat(Idx::bitwidth2size(32)), w.nat(params.size()), w.tuple(params)});
}

/// Returns a fully applied affine_for axiom.
/// See documentation for %affine.For axiom in @ref affine.
// clang-format off
inline const Def* op_for(World& w,
                         Ref begin,
                         Ref end,
                         Ref step,
                         Defs inits,
                         Ref body,
                         Ref brk) {
    auto types = DefVec(inits.size(), [&](size_t i) { return inits[i]->type(); });
    return w.app(fn_for(w, types), {begin, end, step, w.tuple(inits), body, brk});
}
// clang-format on
///@}

} // namespace thorin::plug::affine
