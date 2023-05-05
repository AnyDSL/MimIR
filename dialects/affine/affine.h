#pragma once

#include "thorin/world.h"

#include "dialects/affine/autogen.h"

namespace thorin::affine {

/// @name %%affine.For
///@{
/// Returns the affine_for axiom applied with \a params.
/// See documentation for %affine.For axiom in @ref affine.
inline const Def* fn_for(World& w, Defs params) {
    return w.app(w.annex<affine::For>(),
                 {w.lit_nat(Idx::bitwidth2size(32)), w.lit_nat(params.size()), w.tuple(params)});
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
    DefArray types(inits.size(), [&](size_t i) { return inits[i]->type(); });
    return w.app(fn_for(w, types), {begin, end, step, w.tuple(inits), body, brk});
}
// clang-format on
///@}

} // namespace thorin::affine
