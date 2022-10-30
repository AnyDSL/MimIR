#pragma once

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/math/autogen.h"

namespace thorin::math {

namespace Mode {
enum Mode : nat_t {
    none = 0,
    nnan = 1 << 0,     ///< No NaNs.
                       ///< Allow optimizations to assume the arguments and result are not NaN.
                       ///< Such optimizations are required to retain defined behavior over NaNs,
                       ///< but the value of the result is undefined.
    ninf = 1 << 1,     ///< No Infs.
                       ///> Allow optimizations to assume the arguments and result are not +/-Inf.
                       ///< Such optimizations are required to retain defined behavior over +/-Inf,
                       ///< but the value of the result is undefined.
    nsz = 1 << 2,      ///< No Signed Zeros.
                       ///< Allow optimizations to treat the sign of a zero argument or result as insignificant.
    arcp = 1 << 3,     ///< Allow Reciprocal.
                       ///< Allow optimizations to use the reciprocal of an argument rather than perform division.
    contract = 1 << 4, ///< Allow floating-point contraction.
                       ///< (e.g. fusing a multiply followed by an addition into a fused multiply-and-add).
    afn = 1 << 5,      ///< Approximate functions.
                       ///< Allow substitution of approximate calculations for functions (sin, log, sqrt, etc).
    reassoc = 1 << 6,  ///< Allow reassociation transformations for floating-point operations.
                       ///< This may dramatically change results in floating point.
    finite = nnan | ninf,
    unsafe = nsz | arcp | reassoc,
    fast   = nnan | ninf | nsz | arcp | contract | afn | reassoc,
    bot    = fast,
    top    = none,
};
}

inline const Def* finfer(const Def* def) { return force<F>(def->type())->arg(); }

/// @name fn - these guys yield the final function to be invoked for the various operations
///@{
inline const Def* fn(arith o, const Def* pe, const Def* mode, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(w.app(w.ax(o), pe, dbg), mode, dbg);
}
inline const Def* fn(x o, const Def* pe, const Def* mode, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(w.app(w.ax(o), pe, dbg), mode, dbg);
}
inline const Def* fn(pow o, const Def* pe, const Def* mode, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(w.app(w.ax(o), pe, dbg), mode, dbg);
}
inline const Def* fn(rt o, const Def* pe, const Def* mode, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(w.app(w.ax(o), pe, dbg), mode, dbg);
}
inline const Def* fn(tri o, const Def* pe, const Def* mode, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(w.app(w.ax(o), pe, dbg), mode, dbg);
}
inline const Def* fn(exp o, const Def* pe, const Def* mode, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(w.app(w.ax(o), pe, dbg), mode, dbg);
}
inline const Def* fn(er o, const Def* pe, const Def* mode, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(w.app(w.ax(o), pe, dbg), mode, dbg);
}
inline const Def* fn(cmp o, const Def* pe, const Def* mode, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(w.app(w.ax(o), pe, dbg), mode, dbg);
}
inline const Def* fn(conv o, const Def* src_s, const Def* dst_s, const Def* dbg = {}) {
    World& w = src_s->world();
    return w.app(w.app(w.ax(o), src_s), dst_s, dbg);
}
///@}

/// @name op - these guys build the final function application for the various operations
///@{
inline const Def* op(arith o, const Def* mode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(fn(o, finfer(a), mode), {a, b}, dbg);
}
inline const Def* op(x o, const Def* mode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(fn(o, finfer(a), mode), {a, b}, dbg);
}
inline const Def* op(pow o, const Def* mode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(fn(o, finfer(a), mode), {a, b}, dbg);
}
inline const Def* op(rt o, const Def* mode, const Def* a, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(fn(o, finfer(a), mode), a, dbg);
}
inline const Def* op(tri o, const Def* mode, const Def* a, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(fn(o, finfer(a), mode), a, dbg);
}
inline const Def* op(exp o, const Def* mode, const Def* a, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(fn(o, finfer(a), mode), a, dbg);
}
inline const Def* op(er o, const Def* mode, const Def* a, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(fn(o, finfer(a), mode), a, dbg);
}
inline const Def* op(cmp o, const Def* mode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = mode->world();
    return w.app(fn(o, finfer(a), mode), {a, b}, dbg);
}
inline const Def* op(conv o, const Def* dst_t, const Def* src, const Def* dbg = {}) {
    World& w = dst_t->world();
    auto d   = dst_t->as<App>()->arg();
    auto s   = src->type()->as<App>()->arg();
    return w.app(fn(o, s, d), src, dbg);
}
template<class O>
const Def* op(O o, nat_t mode, const Def* a, const Def* b, const Def* dbg = {}) {
    World& w = a->world();
    return op(o, w.lit_nat(mode), a, b, dbg);
}

template<nat_t P, nat_t E>
inline auto match_f(const Def* def) {
    if (auto f_ty = match<F>(def)) {
        if (auto [p, e] = f_ty->arg()->projs<2>(isa_lit<nat_t>); p && e && *p == P && *e == E) return f_ty;
    }
    return Match<F, App>();
}
inline auto match_f16(const Def* def) { return match_f<10, 5>(def); }
inline auto match_f32(const Def* def) { return match_f<23, 8>(def); }
inline auto match_f64(const Def* def) { return match_f<52, 11>(def); }
inline std::optional<nat_t> isa_f(const Def* def) {
    if (auto f_ty = match<F>(def)) {
        if (auto [p, e] = f_ty->arg()->projs<2>(isa_lit<nat_t>); p && e) {
            if (*p == 10 && e == 5) return 16;
            if (*p == 23 && e == 8) return 32;
            if (*p == 52 && e == 11) return 64;
        }
    }
    return {};
}
///@}

/// @name type_f
///@{
inline const Axiom* type_f(World& w) { return w.ax<F>(); }
inline const Def* type_f(const Def* pe) {
    World& w = pe->world();
    return w.app(type_f(w), pe);
}
inline const Def* type_f(World& w, nat_t p, nat_t e) {
    auto lp = w.lit_nat(p);
    auto le = w.lit_nat(e);
    return type_f(w.tuple({lp, le}));
}
inline const Def* type_f16(World& w) { return type_f(w, 10, 5); }
inline const Def* type_f32(World& w) { return type_f(w, 23, 8); }
inline const Def* type_f64(World& w) { return type_f(w, 52, 11); }
///@}

/// @name lit_f
///@{
// clang-format off
template<class R>
const Lit* lit_f(World& w, R val, const Def* dbg = {}) {
    static_assert(std::is_floating_point<R>() || std::is_same<R, f16>());
    if constexpr (false) {}
    else if constexpr (sizeof(R) == 2) return w.lit(type_f16(w), thorin::bitcast<u16>(val), dbg);
    else if constexpr (sizeof(R) == 4) return w.lit(type_f32(w), thorin::bitcast<u32>(val), dbg);
    else if constexpr (sizeof(R) == 8) return w.lit(type_f64(w), thorin::bitcast<u64>(val), dbg);
    else unreachable();
}

inline const Lit* lit_f(World& w, nat_t width, f64 val, const Def* dbg = {}) {
    switch (width) {
        case 16: assert(f64(f16(f32(val))) == val && "loosing precision"); return lit_f(w, f16(f32(val)), dbg);
        case 32: assert(f64(f32(   (val))) == val && "loosing precision"); return lit_f(w, f32(   (val)), dbg);
        case 64: assert(f64(f64(   (val))) == val && "loosing precision"); return lit_f(w, f64(   (val)), dbg);
        default: unreachable();
    }
}
// clang-format on
///@}

/// @name wrappers for unary operations
///@{
inline const Def* op_rminus(const Def* mode, const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    auto s   = isa_f(a->type());
    return op(arith::sub, mode, lit_f(w, *s, -0.0), a, dbg);
}
inline const Def* op_rminus(nat_t mode, const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    return op_rminus(w.lit_nat(mode), a, dbg);
}
///@}

} // namespace thorin::math

namespace thorin {

/// @name is_commutative/is_associative
///@{
// clang-format off
constexpr bool is_commutative(math::x       ) { return true; }
constexpr bool is_commutative(math::arith id) { return id == math::arith ::add || id == math::arith::mul; }
constexpr bool is_commutative(math::cmp   id) { return id == math::cmp   ::e   || id == math::cmp  ::ne ; }
// clang-format off
///@}

} // namespace thorin
