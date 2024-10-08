#pragma once

#include <cmath>
#include <cstdint>

#include <limits>
#include <ostream>
#include <type_traits>

#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wmismatched-tags"
#endif
#define HALF_ROUND_STYLE        1
#define HALF_ROUND_TIES_TO_EVEN 1
#include <half.hpp>
#ifdef __clang__
#    pragma clang diagnostic pop
#endif

namespace mim {
// clang-format off

#define MIM_1_8_16_32_64(m) m(1) m(8) m(16) m(32) m(64)
#define MIM_8_16_32_64(m)        m(8) m(16) m(32) m(64)
#define MIM_16_32_64(m)               m(16) m(32) m(64)

/// @name Aliases for some Base Types
///@{
// using CODE1, CODE2, ... here as a workaround for Doxygen
#define CODE1(i)                   \
    using s ## i =  int ## i ##_t; \
    using u ## i = uint ## i ##_t;
MIM_8_16_32_64(CODE1)
#undef CODE1

using half_float::half;
using u1       = bool;
using f16      = half;
using f32      = float;
using f64      = double;
using level_t  = u64;
using nat_t    = u64;
using node_t   = u8;
using flags_t  = u64;
using plugin_t = u64;
using tag_t    = u8;
using sub_t    = u8;
///@}

namespace detail {
template<int> struct w2u_ { using type = void; };
template<int> struct w2s_ { using type = void; };
template<int> struct w2f_ { using type = void; };
template<> struct w2u_<1> { using type = bool; }; ///< Map both signed 1 and unsigned 1 to `bool`.
template<> struct w2s_<1> { using type = bool; }; ///< See above.

#define CODE2(i)                                        \
    template<> struct w2u_<i> { using type = u ## i; }; \
    template<> struct w2s_<i> { using type = s ## i; };
MIM_8_16_32_64(CODE2)
#undef CODE2

#define CODE3(i)                                        \
    template<> struct w2f_<i> { using type = f ## i; };
MIM_16_32_64(CODE3)
#undef CODE3
} // namespace detail

/// @name Width to Signed/Unsigned/Float
///@{
template<int w> using w2u = typename detail::w2u_<w>::type;
template<int w> using w2s = typename detail::w2s_<w>::type;
template<int w> using w2f = typename detail::w2f_<w>::type;
///@}

/// @name User-Defined Literals
///@{
#define CODE4(i)                                                                        \
    constexpr s ## i operator"" _s ## i(unsigned long long int s) { return s ## i(s); } \
    constexpr u ## i operator"" _u ## i(unsigned long long int u) { return u ## i(u); }
MIM_8_16_32_64(CODE4)
#undef CODE4

/// A `size_t` literal. Use `0_s` to disambiguate `0` from `nullptr`.
constexpr size_t operator""_s(unsigned long long int i) { return size_t(i); }
constexpr nat_t operator""_n(unsigned long long int i) { return nat_t(i); }
inline /*constexpr*/ f16 operator""_f16(long double d) { return f16(float(d)); } // wait till fixed upstream
constexpr f32 operator""_f32(long double d) { return f32(d); }
constexpr f64 operator""_f64(long double d) { return f64(d); }
///@}

/// @name rem
///@{
inline half        rem(half        a, half        b) { return      fmod(a, b); }
inline float       rem(float       a, float       b) { return std::fmod(a, b); }
inline double      rem(double      a, double      b) { return std::fmod(a, b); }
inline long double rem(long double a, long double b) { return std::fmod(a, b); }
///@}

// clang-format on
} // namespace mim
