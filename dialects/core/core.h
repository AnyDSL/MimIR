#pragma once

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/core/autogen.h"

namespace thorin::core {

namespace WMode {
enum : nat_t {
    none = 0,
    nsw  = 1 << 0,
    nuw  = 1 << 1,
};
}

namespace RMode {
enum RMode : nat_t {
    none = 0,
    nnan = 1 << 0, ///< No NaNs - Allow optimizations to assume the arguments and result are not NaN. Such optimizations
                   ///< are required to retain defined behavior over NaNs, but the value of the result is undefined.
    ninf =
        1 << 1, ///< No Infs - Allow optimizations to assume the arguments and result are not +/-Inf. Such optimizations
                ///< are required to retain defined behavior over +/-Inf, but the value of the result is undefined.
    nsz = 1
       << 2, ///< No Signed Zeros - Allow optimizations to treat the sign of a zero argument or result as insignificant.
    arcp = 1 << 3, ///< Allow Reciprocal - Allow optimizations to use the reciprocal of an argument rather than perform
                   ///< division.
    contract = 1 << 4, ///< Allow floating-point contraction (e.g. fusing a multiply followed by an addition into a
                       ///< fused multiply-and-add).
    afn = 1 << 5, ///< Approximate functions - Allow substitution of approximate calculations for functions (sin, log,
                  ///< sqrt, etc). See floating-point intrinsic definitions for places where this can apply to LLVM’s
                  ///< intrinsic math functions.
    reassoc = 1 << 6, ///< Allow reassociation transformations for floating-point operations. This may dramatically
                      ///< change results in floating point.
    finite = nnan | ninf,
    unsafe = nsz | arcp | reassoc,
    fast   = nnan | ninf | nsz | arcp | contract | afn | reassoc,
    bot    = fast,
    top    = none,
};
}

inline const Def* rinfer(const Def* def) { return force<Real>(def->type())->arg(); }

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
inline const Def* fn(rop o, const Def* rmode, const Def* width, const Def* dbg = {}) {
    World& w = rmode->world();
    return w.app(w.ax(o), {rmode, width}, dbg);
}
inline const Def* fn(rcmp o, const Def* rmode, const Def* width, const Def* dbg = {}) {
    World& w = rmode->world();
    return w.app(w.ax(o), {rmode, width}, dbg);
}
inline const Def* fn(conv o, const Def* dst_w, const Def* src_w, const Def* dbg = {}) {
    World& w = dst_w->world();
    return w.app(w.ax(o), {dst_w, src_w}, dbg);
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
inline const Def* op(rop o, const Def* rmode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = rmode->world();
    return w.app(fn(o, rmode, rinfer(a)), {a, b}, dbg);
}
inline const Def* op(rcmp o, const Def* rmode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = rmode->world();
    return w.app(fn(o, rmode, rinfer(a)), {a, b}, dbg);
}

template<class O>
const Def* op(O o, nat_t mode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return op(o, w.lit_nat(mode), a, b, dbg);
}

inline const Def* get_size(const Def* type) {
    if (auto size = Idx::size(type)) return size;
    if (auto real = match<Real>(type)) return real->arg();
    unreachable();
}

inline const Def* op(conv o, const Def* dst_type, const Def* src, const Def* dbg = {}) {
    World& w = dst_type->world();
    auto d   = get_size(dst_type);
    auto s   = get_size(src->type());
    return w.app(fn(o, d, s), src, dbg);
}
inline const Def* op(trait o, const Def* type, const Def* dbg = {}) {
    World& w = type->world();
    return w.app(w.ax(o), type, dbg);
}
inline const Def* op_bitcast(const Def* dst_type, const Def* src, const Def* dbg = {}) {
    World& w = dst_type->world();
    return w.app(fn_bitcast(dst_type, src->type()), src, dbg);
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

/// @name type_real
///@{
inline const Axiom* type_real(World& w) { return w.ax<Real>(); }
inline const Def* type_real(const Def* width) {
    World& w = width->world();
    return w.app(type_real(w), width);
}
inline const Def* type_real(World& w, nat_t width) { return type_real(w.lit_nat(width)); }
///@}

/// @name lit_real
///@{
// clang-format off
template<class R>
const Lit* lit_real(World& w, R val, const Def* dbg = {}) {
    static_assert(std::is_floating_point<R>() || std::is_same<R, r16>());
    if constexpr (false) {}
    else if constexpr (sizeof(R) == 2) return w.lit(type_real(w, 16), thorin::bitcast<u16>(val), dbg);
    else if constexpr (sizeof(R) == 4) return w.lit(type_real(w, 32), thorin::bitcast<u32>(val), dbg);
    else if constexpr (sizeof(R) == 8) return w.lit(type_real(w, 64), thorin::bitcast<u64>(val), dbg);
    else unreachable();
}

inline const Lit* lit_real(World& w, nat_t width, r64 val, const Def* dbg = {}) {
    switch (width) {
        case 16: assert(r64(r16(r32(val))) == val && "loosing precision"); return lit_real(w, r16(r32(val)), dbg);
        case 32: assert(r64(r32(   (val))) == val && "loosing precision"); return lit_real(w, r32(   (val)), dbg);
        case 64: assert(r64(r64(   (val))) == val && "loosing precision"); return lit_real(w, r64(   (val)), dbg);
        default: unreachable();
    }
}
// clang-format on
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
inline const Def* op_rminus(const Def* rmode, const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    auto s   = as_lit(rinfer(a));
    return op(rop::sub, rmode, lit_real(w, s, -0.0), a, dbg);
}
inline const Def* op_rminus(nat_t rmode, const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    return op_rminus(w.lit_nat(rmode), a, dbg);
}
///@}

/// Use like this:
/// `a op b = tab[a][b]`
inline std::array<std::array<u64, 2>, 2> make_truth_table(bit2 id) {
    return {
        {{sub_t(id) & sub_t(0b0001) ? u64(-1) : 0, sub_t(id) & sub_t(0b0100) ? u64(-1) : 0},
         {sub_t(id) & sub_t(0b0010) ? u64(-1) : 0, sub_t(id) & sub_t(0b1000) ? u64(-1) : 0}}
    };
}

template<bool up>
const Sigma* convert(const TBound<up>* b);

inline const Sigma* convert(const Bound* b) { return b->isa<Join>() ? convert(b->as<Join>()) : convert(b->as<Meet>()); }

} // namespace thorin::core
