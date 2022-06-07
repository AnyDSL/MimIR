#ifndef DIALECTS_CORE_CORE_H
#define DIALECTS_CORE_CORE_H

#include "thorin/world.h"

#include "dialects/core.h"

namespace thorin::core {

/// @name fn - these guys yield the final function to be invoked for the various operations
///@{
inline const Def* fn(bit2 o, const Def* mod, const Def* dbg = {}) {
    World& w = mod->world();
    return w.app(w.ax(o), mod, dbg);
}

inline const Def* fn(icmp o, const Def* mod, const Def* dbg = {}) {
    World& w = mod->world();
    return w.app(w.ax(o), mod, dbg);
}
inline const Def* fn(shr o, const Def* mod, const Def* dbg = {}) {
    World& w = mod->world();
    return w.app(w.ax(o), mod, dbg);
}
inline const Def* fn(wrap o, const Def* wmode, const Def* mod, const Def* dbg = {}) {
    World& w = mod->world();
    return w.app(w.ax(o), {wmode, mod}, dbg);
}
///@}

/// @name op - these guys build the final function application for the various operations
///@{
inline const Def* op(bit2 o, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(fn(o, w.infer(a)), {a, b}, dbg);
}
inline const Def* op(icmp o, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(fn(o, w.infer(a)), {a, b}, dbg);
}

inline const Def* op(shr o, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(fn(o, w.infer(a)), {a, b}, dbg);
}
inline const Def* op(wrap o, const Def* wmode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(fn(o, wmode, w.infer(a)), {a, b}, dbg);
}

template<class O>
const Def* op(O o, nat_t mode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return op(o, w.lit_nat(mode), a, b, dbg);
}
///@}

/// @name wrappers for unary operations
///@{
inline const Def* op_negate(const Def* a, const Def* dbg = {}) {
    World& w   = a->world();
    auto width = as_lit(w.infer(a));
    return op(bit2::_xor, w.lit_int(width, width - 1_u64), a, dbg);
}
// todo: real op
// const Def* op_rminus(const Def* rmode, const Def* a, const Def* dbg = {}) {
//     World& w   = rmode->world();
//     auto width = as_lit(w.infer(a));
//     return op(ROp::sub, rmode, w.lit_real(width, -0.0), a, dbg);
// }
inline const Def* op_wminus(const Def* wmode, const Def* a, const Def* dbg = {}) {
    World& w   = a->world();
    auto width = as_lit(w.infer(a));
    return op(wrap::sub, wmode, w.lit_int(width, 0), a, dbg);
}
// todo: real op
// const Def* op_rminus(nat_t rmode, const Def* a, const Def* dbg = {}) { return op_rminus(lit_nat(rmode), a, dbg); }
inline const Def* op_wminus(nat_t wmode, const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    return op_wminus(w.lit_nat(wmode), a, dbg);
}
///@}

} // namespace thorin::core

#endif
