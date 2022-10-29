#pragma once

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/core/autogen.h"

namespace thorin::core {

namespace Mode {
enum : nat_t {
    none = 0,
    nsw  = 1 << 0,
    nuw  = 1 << 1,
};
}

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
inline const Def* fn(div o, const Def* mod, const Def* dbg = {}) {
    World& w = mod->world();
    return w.app(w.ax(o), mod, dbg);
}
inline const Def* fn(conv o, const Def* src_s, const Def* dst_s, const Def* dbg = {}) {
    World& w = src_s->world();
    return w.app(w.app(w.ax(o), src_s, dbg), dst_s, dbg);
}
inline const Def* fn_bitcast(const Def* dst_t, const Def* src_t, const Def* dbg = {}) {
    World& w = dst_t->world();
    return w.app(w.ax<bitcast>(), {dst_t, src_t}, dbg);
}
///@}

/// @name op - these guys build the final function application for the various operations
///@{
inline const Def* op(nop o, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(w.ax(o), {a, b}, dbg);
}
inline const Def* op(bit2 o, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(fn(o, w.iinfer(a)), {a, b}, dbg);
}
inline const Def* op(icmp o, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(fn(o, w.iinfer(a)), {a, b}, dbg);
}
inline const Def* op(shr o, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(fn(o, w.iinfer(a)), {a, b}, dbg);
}
inline const Def* op(wrap o, const Def* wmode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return w.app(fn(o, wmode, w.iinfer(a)), {a, b}, dbg);
}
inline const Def* op(div o, const Def* mem, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = mem->world();
    return w.app(fn(o, w.iinfer(a)), {mem, a, b}, dbg);
}

template<class O>
const Def* op(O o, nat_t mode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return op(o, w.lit_nat(mode), a, b, dbg);
}

inline const Def* op(conv o, const Def* dst_t, const Def* src, const Def* dbg = {}) {
    World& w = dst_t->world();
    auto d   = Idx::size(dst_t);
    auto s   = Idx::size(src->type());
    return w.app(fn(o, s, d), src, dbg);
}
inline const Def* op(trait o, const Def* type, const Def* dbg = {}) {
    World& w = type->world();
    return w.app(w.ax(o), type, dbg);
}
inline const Def* op_bitcast(const Def* dst_t, const Def* src, const Def* dbg = {}) {
    World& w = dst_t->world();
    return w.app(fn_bitcast(dst_t, src->type()), src, dbg);
}
inline const Def* op(pe o, const Def* def, const Def* dbg = {}) {
    World& w = def->world();
    return w.app(w.app(w.ax(o), def->type()), def, dbg);
}
///@}

/// @name extract helper
///@{
inline const Def* extract_unsafe(const Def* d, const Def* i, const Def* dbg = {}) {
    World& w = d->world();
    return w.extract(d, op(conv::u2u, w.type_idx(as_lit(d->unfold_type()->arity())), i, dbg), dbg);
}
inline const Def* extract_unsafe(const Def* d, u64 i, const Def* dbg = {}) {
    World& w = d->world();
    return extract_unsafe(d, w.lit_idx(0_u64, i), dbg);
}
///@}

/// @name insert helper
///@{
inline const Def* insert_unsafe(const Def* d, const Def* i, const Def* val, const Def* dbg = {}) {
    World& w = d->world();
    return w.insert(d, op(conv::u2u, w.type_idx(as_lit(d->unfold_type()->arity())), i), val, dbg);
}
inline const Def* insert_unsafe(const Def* d, u64 i, const Def* val, const Def* dbg = {}) {
    World& w = d->world();
    return insert_unsafe(d, w.lit_idx(0_u64, i), val, dbg);
}
///@}

/// @name wrappers for unary operations
///@{
inline const Def* op_negate(const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    auto s   = as_lit(w.iinfer(a));
    return op(bit2::_xor, w.lit_idx(s, s - 1_u64), a, dbg);
}
inline const Def* op_wminus(const Def* wmode, const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    auto s   = as_lit(w.iinfer(a));
    return op(wrap::sub, wmode, w.lit_idx(s, 0), a, dbg);
}
inline const Def* op_wminus(nat_t wmode, const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    return op_wminus(w.lit_nat(wmode), a, dbg);
}
///@}

/// Use like this:
/// `a op b = tab[a][b]`
constexpr std::array<std::array<u64, 2>, 2> make_truth_table(bit2 id) {
    return {
        {{sub_t(id) & sub_t(0b0001) ? u64(-1) : 0, sub_t(id) & sub_t(0b0100) ? u64(-1) : 0},
         {sub_t(id) & sub_t(0b0010) ? u64(-1) : 0, sub_t(id) & sub_t(0b1000) ? u64(-1) : 0}}
    };
}

template<bool up>
const Sigma* convert(const TBound<up>* b);

inline const Sigma* convert(const Bound* b) { return b->isa<Join>() ? convert(b->as<Join>()) : convert(b->as<Meet>()); }

} // namespace thorin::core
