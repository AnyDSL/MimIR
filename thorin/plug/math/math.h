#pragma once

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "thorin/plug/math/autogen.h"

namespace thorin::plug::math {

/// @name Mode
///@{
// clang-format off
/// Allowed optimizations for a specific operation.
enum class Mode : nat_t {
    top      = 0,
    none     = top,                ///< Alias for Mode::none.
    nnan     = 1 << 0,             ///< No NaNs.
                                   ///< Allow optimizations to assume the arguments and result are not NaN.
                                   ///< Such optimizations are required to retain defined behavior over NaNs, but the value of the result is undefined.
    ninf     = 1 << 1,             ///< No Infs.
                                   ///< Allow optimizations to assume the arguments and result are not +/-Inf.
                                   ///< Such optimizations are required to retain defined behavior over +/-Inf, but the value of the result is undefined.
    nsz      = 1 << 2,             ///< No Signed Zeros.
                                   ///< Allow optimizations to treat the sign of a zero argument or result as insignificant.
    arcp     = 1 << 3,             ///< Allow Reciprocal.
                                   ///< Allow optimizations to use the reciprocal of an argument rather than perform division.
    contract = 1 << 4,             ///< Allow floating-point contraction
                                   ///< (e.g. fusing a multiply followed by an addition into a fused multiply-and-add).
    afn      = 1 << 5,             ///< Approximate functions.
                                   ///< Allow substitution of approximate calculations for functions (sin, log, sqrt, etc).
    reassoc  = 1 << 6,             ///< Allow reassociation transformations for floating-point operations.
                                   ///< This may dramatically change results in floating point.
    finite = nnan | ninf,          ///< Mode::nnan `|` Mode::ninf.
    unsafe = nsz | arcp | reassoc, ///< Mode::nsz `|` Mode::arcp `|` Mode::reassoc
    fast   = nnan | ninf | nsz
           | arcp | contract | afn
           | reassoc,              ///< All flags.
    bot    = fast,                 ///< Alias for Mode::fast.
};
// clang-format on

THORIN_ENUM_OPERATORS(Mode)

/// Give Mode as thorin::plug::math::Mode, thorin::nat_t or Ref.
using VMode = std::variant<Mode, nat_t, Ref>;

/// thorin::plug::math::VMode -> Ref.
inline Ref mode(World& w, VMode m) {
    if (auto def = std::get_if<Ref>(&m)) return *def;
    if (auto nat = std::get_if<nat_t>(&m)) return w.lit_nat(*nat);
    return w.lit_nat((nat_t)std::get<Mode>(m));
}
///@}

/// @name %%math.F
///@{
inline Ref type_f(Ref pe) {
    World& w = pe->world();
    return w.app(w.annex<F>(), pe);
}
inline Ref type_f(World& w, nat_t p, nat_t e) {
    auto lp = w.lit_nat(p);
    auto le = w.lit_nat(e);
    return type_f(w.tuple({lp, le}));
}
template<nat_t P, nat_t E> inline auto match_f(Ref def) {
    if (auto f_ty = match<F>(def)) {
        auto [p, e] = f_ty->arg()->projs<2>([](auto op) { return Lit::isa(op); });
        if (p && e && *p == P && *e == E) return f_ty;
    }
    return Match<F, App>();
}

inline auto match_f16(Ref def) { return match_f<10, 5>(def); }
inline auto match_f32(Ref def) { return match_f<23, 8>(def); }
inline auto match_f64(Ref def) { return match_f<52, 11>(def); }

inline std::optional<nat_t> isa_f(Ref def) {
    if (auto f_ty = match<F>(def)) {
        if (auto [p, e] = f_ty->arg()->projs<2>([](auto op) { return Lit::isa(op); }); p && e) {
            if (*p == 10 && e == 5) return 16;
            if (*p == 23 && e == 8) return 32;
            if (*p == 52 && e == 11) return 64;
        }
    }
    return {};
}

// clang-format off
template<class R>
const Lit* lit_f(World& w, R val) {
    static_assert(std::is_floating_point<R>() || std::is_same<R, thorin::f16>());
    if constexpr (false) {}
    else if constexpr (sizeof(R) == 2) return w.lit(w.annex<F16>(), thorin::bitcast<u16>(val));
    else if constexpr (sizeof(R) == 4) return w.lit(w.annex<F32>(), thorin::bitcast<u32>(val));
    else if constexpr (sizeof(R) == 8) return w.lit(w.annex<F64>(), thorin::bitcast<u64>(val));
    else fe::unreachable();
}

inline const Lit* lit_f(World& w, nat_t width, thorin::f64 val) {
    switch (width) {
        case 16: assert(thorin::f64(thorin::f16(thorin::f32(val))) == val && "loosing precision"); return lit_f(w, thorin::f16(thorin::f32(val)));
        case 32: assert(thorin::f64(thorin::f32(           (val))) == val && "loosing precision"); return lit_f(w, thorin::f32(   (val)));
        case 64: assert(thorin::f64(thorin::f64(           (val))) == val && "loosing precision"); return lit_f(w, thorin::f64(   (val)));
        default: fe::unreachable();
    }
}
// clang-format on
///@}

/// @name %%math.arith
///@{
inline Ref op_rminus(VMode m, Ref a) {
    World& w = a->world();
    auto s   = isa_f(a->type());
    return w.call(arith::sub, mode(w, m), Defs{lit_f(w, *s, -0.0), a});
}
///@}

} // namespace thorin::plug::math

namespace thorin {

/// @name is_commutative/is_associative
///@{
// clang-format off
constexpr bool is_commutative(plug::math::extrema ) { return true; }
constexpr bool is_commutative(plug::math::arith id) { return id == plug::math::arith ::add || id == plug::math::arith::mul; }
constexpr bool is_commutative(plug::math::cmp   id) { return id == plug::math::cmp   ::e   || id == plug::math::cmp  ::ne ; }
// clang-format off
///@}

} // namespace thorin
