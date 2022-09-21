#pragma once

#include <string_view>

#include "thorin/util/print.h"
#include "thorin/util/types.h"

// clang-format off
namespace thorin {

using nat_t     = u64;
using node_t    = u8;
using flags_t   = u64;
using dialect_t = u64;
using tag_t     = u8;
using sub_t     = u8;

#define THORIN_NODE(m)                                                        \
    m(Type, type)       m(Univ, univ)                                         \
    m(Pi, pi)           m(Lam, lam)     m(App, app)                           \
    m(Sigma, sigma)     m(Tuple, tuple) m(Extract, extract) m(Insert, insert) \
    m(Arr, arr)         m(Pack, pack)                                         \
    m(Join, join)       m(Vel, vel)     m(Test, test)       m(Top, top)       \
    m(Meet, meet)       m(Ac,  ac )     m(Pick, pick)       m(Bot, bot)       \
    m(Proxy, proxy)                                                           \
    m(Axiom, axiom)                                                           \
    m(Lit, lit)                                                               \
    m(Nat, nat)         m(Idx, int)                                           \
    m(Var, var)                                                               \
    m(Infer, infer)                                                           \
    m(Global, global)                                                         \
    m(Singleton, singleton)

#define THORIN_TAG(m)                                                      \
    m(Mem, mem) m(Real, real) m(Ptr, ptr)                                  \
    m(Bit, bit) m(Shr, shr) m(Wrap, wrap) m(ROp, rop)                      \
    m(ICmp, icmp) m(RCmp, rcmp)                                            \
    m(Trait, trait) m(Conv, conv) m(PE, pe) m(Acc, acc)                    \
    m(Bitcast, bitcast) m(LEA, lea)                                        \
    m(Alloc, alloc) m(Slot, slot) m(Malloc, malloc) m(Mslot, mslot)        \
    m(Load, load) m(Remem, remem) m(Store, store)                          \
    m(Atomic, atomic)                                                      \
    m(Zip, zip) m(For, affine_for)                                         \
    m(RevDiff, rev_diff) m(TangentVector, tangent_vector)                  \

namespace WMode {
enum : nat_t {
    none = 0,
    nsw  = 1 << 0,
    nuw  = 1 << 1,
};
}

namespace RMode {
enum RMode : nat_t {
    none     = 0,
    nnan     = 1 << 0, ///< No NaNs - Allow optimizations to assume the arguments and result are not NaN. Such optimizations are required to retain defined behavior over NaNs, but the value of the result is undefined.
    ninf     = 1 << 1, ///< No Infs - Allow optimizations to assume the arguments and result are not +/-Inf. Such optimizations are required to retain defined behavior over +/-Inf, but the value of the result is undefined.
    nsz      = 1 << 2, ///< No Signed Zeros - Allow optimizations to treat the sign of a zero argument or result as insignificant.
    arcp     = 1 << 3, ///< Allow Reciprocal - Allow optimizations to use the reciprocal of an argument rather than perform division.
    contract = 1 << 4, ///< Allow floating-point contraction (e.g. fusing a multiply followed by an addition into a fused multiply-and-add).
    afn      = 1 << 5, ///< Approximate functions - Allow substitution of approximate calculations for functions (sin, log, sqrt, etc). See floating-point intrinsic definitions for places where this can apply to LLVM’s intrinsic math functions.
    reassoc  = 1 << 6, ///< Allow reassociation transformations for floating-point operations. This may dramatically change results in floating point.
    finite   = nnan | ninf,
    unsafe   = nsz | arcp | reassoc,
    fast = nnan | ninf | nsz | arcp | contract | afn | reassoc,
    bot = fast,
    top = none,
};
}

/// Integer operations that neither take a @p WMode nor do produce a side effect - arithmetic or logical shift right.
#define THORIN_SHR(m) m(Shr, ashr) m(Shr, lshr)
/// Integer operations that might wrap and, hence, take @p WMode.
#define THORIN_WRAP(m) m(Wrap, add) m(Wrap, sub) m(Wrap, mul) m(Wrap, shl)
/// Floating point (real) operations that take @p RMode.
#define THORIN_R_OP(m) m(ROp, add) m(ROp, sub) m(ROp, mul) m(ROp, div) m(ROp, rem)
/// Type traits
#define THORIN_TRAIT(m) m(Trait, size) m(Trait, align)
/// Conversions
#define THORIN_CONV(m) m(Conv, s2s) m(Conv, u2u) m(Conv, s2r) m(Conv, u2r) m(Conv, r2s) m(Conv, r2u) m(Conv, r2r)
/// Partial Evaluation related operations
#define THORIN_PE(m) m(PE, hlt) m(PE, known) m(PE, run)
/// Accelerators
#define THORIN_ACC(m) m(Acc, vecotrize) m(Acc, parallel) m(Acc, opencl) m(Acc, cuda) m(Acc, nvvm) m (Acc, amdgpu)


/// The 5 relations are disjoint and are organized as follows:
/// ```
///              ----
///              4321
///          01234567
///          ////////
///          00001111
///    y→    00110011
///          01010101
///
///  x 0/000 ELLLXXXX
///  ↓ 1/001 GELLXXXX
///    2/010 GGELXXXX
///    3/011 GGGEXXXX
/// -4/4/100 YYYYELLL
/// -3/5/101 YYYYGELL
/// -2/6/110 YYYYGGEL   X = plus, minus
/// -1/7/111 YYYYGGGE   Y = minus, plus
/// ```
/// The more obscure combinations are prefixed with @c _.
/// The standard comparisons front ends want to use, don't have this prefix.
#define THORIN_I_CMP(m)              /* X Y G L E                                                   */ \
                     m(ICmp,   _f)   /* o o o o o - always false                                    */ \
                     m(ICmp,    e)   /* o o o o x - equal                                           */ \
                     m(ICmp,   _l)   /* o o o x o - less (same sign)                                */ \
                     m(ICmp,  _le)   /* o o o x x - less or equal                                   */ \
                     m(ICmp,   _g)   /* o o x o o - greater (same sign)                             */ \
                     m(ICmp,  _ge)   /* o o x o x - greater or equal                                */ \
                     m(ICmp,  _gl)   /* o o x x o - greater or less                                 */ \
                     m(ICmp, _gle)   /* o o x x x - greater or less or equal == same sign           */ \
                     m(ICmp,   _y)   /* o x o o o - minus plus                                      */ \
                     m(ICmp,  _ye)   /* o x o o x - minus plus or equal                             */ \
                     m(ICmp,   sl)   /* o x o x o - signed less                                     */ \
                     m(ICmp,  sle)   /* o x o x x - signed less or equal                            */ \
                     m(ICmp,   ug)   /* o x x o o - unsigned greater                                */ \
                     m(ICmp,  uge)   /* o x x o x - unsigned greater or equal                       */ \
                     m(ICmp, _ygl)   /* o x x x o - minus plus or greater or less                   */ \
                     m(ICmp,  _nx)   /* o x x x x - not plus minus                                  */ \
                     m(ICmp,   _x)   /* x o o o o - plus minus                                      */ \
                     m(ICmp,  _xe)   /* x o o o x - plus minus or equal                             */ \
                     m(ICmp,   ul)   /* x o o x o - unsigned less                                   */ \
                     m(ICmp,  ule)   /* x o o x x - unsigned less or equal                          */ \
                     m(ICmp,   sg)   /* x o x o o - signed greater                                  */ \
                     m(ICmp,  sge)   /* x o x o x - signed greater or equal                         */ \
                     m(ICmp, _xgl)   /* x o x x o - greater or less or plus minus                   */ \
                     m(ICmp,  _ny)   /* x o x x x - not minus plus                                  */ \
                     m(ICmp,  _xy)   /* x x o o o - different sign                                  */ \
                     m(ICmp, _xye)   /* x x o o x - different sign or equal                         */ \
                     m(ICmp, _xyl)   /* x x o x o - signed or unsigned less                         */ \
                     m(ICmp,  _ng)   /* x x o x x - signed or unsigned less or equal == not greater */ \
                     m(ICmp, _xyg)   /* x x x o o - signed or unsigned greater                      */ \
                     m(ICmp,  _nl)   /* x x x o x - signed or unsigned greater or equal == not less */ \
                     m(ICmp,   ne)   /* x x x x o - not equal                                       */ \
                     m(ICmp,   _t)   /* x x x x x - always true                                     */

#define THORIN_R_CMP(m)           /* U G L E                                 */ \
                     m(RCmp,   f) /* o o o o - always false                  */ \
                     m(RCmp,   e) /* o o o x - ordered and equal             */ \
                     m(RCmp,   l) /* o o x o - ordered and less              */ \
                     m(RCmp,  le) /* o o x x - ordered and less or equal     */ \
                     m(RCmp,   g) /* o x o o - ordered and greater           */ \
                     m(RCmp,  ge) /* o x o x - ordered and greater or equal  */ \
                     m(RCmp,  ne) /* o x x o - ordered and not equal         */ \
                     m(RCmp,   o) /* o x x x - ordered (no NaNs)             */ \
                     m(RCmp,   u) /* x o o o - unordered (either NaNs)       */ \
                     m(RCmp,  ue) /* x o o x - unordered or equal            */ \
                     m(RCmp,  ul) /* x o x o - unordered or less             */ \
                     m(RCmp, ule) /* x o x x - unordered or less or equal    */ \
                     m(RCmp,  ug) /* x x o o - unordered or greater          */ \
                     m(RCmp, uge) /* x x o x - unordered or greater or equal */ \
                     m(RCmp, une) /* x x x o - unordered or not equal        */ \
                     m(RCmp,   t) /* x x x x - always true                   */

/// Table for all binary boolean operations.
/// See https://en.wikipedia.org/wiki/Truth_table#Binary_operations
#define THORIN_BIT(m)            /* B B A A -                              */ \
                   m(Bit,     f) /* o o o o - always false                 */ \
                   m(Bit,   nor) /* o o o x -                              */ \
                   m(Bit, nciff) /* o o x o - not converse implication     */ \
                   m(Bit,    na) /* o o x x - not first argument           */ \
                   m(Bit,  niff) /* o x o o - not implication              */ \
                   m(Bit,    nb) /* o x o x - not second argument          */ \
                   m(Bit,  _xor) /* o x x o -                              */ \
                   m(Bit,  nand) /* o x x x -                              */ \
                   m(Bit,  _and) /* x o o o -                              */ \
                   m(Bit,  nxor) /* x o o x -                              */ \
                   m(Bit,     b) /* x o x o - second argument              */ \
                   m(Bit,   iff) /* x o x x - implication (if and only if) */ \
                   m(Bit,     a) /* x x o o - first argment                */ \
                   m(Bit,  ciff) /* x x o x - converse implication         */ \
                   m(Bit,   _or) /* x x x o -                              */ \
                   m(Bit,     t) /* x x x x - always true                  */

namespace Node {
#define CODE(node, name) node,
enum : node_t { THORIN_NODE(CODE) Max };
#undef CODE
}

namespace Tag {
#define CODE(sub, name) sub,
enum : tag_t { THORIN_TAG(CODE) Max };
#undef CODE
}

#define CODE(T, o) o,
enum class Bit    : sub_t { THORIN_BIT  (CODE) };
enum class Shr    : sub_t { THORIN_SHR  (CODE) };
enum class Wrap   : sub_t { THORIN_WRAP (CODE) };
enum class ROp    : sub_t { THORIN_R_OP (CODE) };
enum class ICmp   : sub_t { THORIN_I_CMP(CODE) };
enum class RCmp   : sub_t { THORIN_R_CMP(CODE) };
enum class Trait  : sub_t { THORIN_TRAIT(CODE) };
enum class Conv   : sub_t { THORIN_CONV (CODE) };
enum class PE     : sub_t { THORIN_PE   (CODE) };
enum class Acc    : sub_t { THORIN_ACC  (CODE) };
#undef CODE

constexpr ICmp operator|(ICmp a, ICmp b) { return ICmp(sub_t(a) | sub_t(b)); }
constexpr ICmp operator&(ICmp a, ICmp b) { return ICmp(sub_t(a) & sub_t(b)); }
constexpr ICmp operator^(ICmp a, ICmp b) { return ICmp(sub_t(a) ^ sub_t(b)); }

constexpr RCmp operator|(RCmp a, RCmp b) { return RCmp(sub_t(a) | sub_t(b)); }
constexpr RCmp operator&(RCmp a, RCmp b) { return RCmp(sub_t(a) & sub_t(b)); }
constexpr RCmp operator^(RCmp a, RCmp b) { return RCmp(sub_t(a) ^ sub_t(b)); }

#define CODE(T, o) case T::o: return #T "_" #o;
constexpr std::string_view op2str(Bit   o) { switch (o) { THORIN_BIT  (CODE) default: unreachable(); } }
constexpr std::string_view op2str(Shr   o) { switch (o) { THORIN_SHR  (CODE) default: unreachable(); } }
constexpr std::string_view op2str(Wrap  o) { switch (o) { THORIN_WRAP (CODE) default: unreachable(); } }
constexpr std::string_view op2str(ROp   o) { switch (o) { THORIN_R_OP (CODE) default: unreachable(); } }
constexpr std::string_view op2str(ICmp  o) { switch (o) { THORIN_I_CMP(CODE) default: unreachable(); } }
constexpr std::string_view op2str(RCmp  o) { switch (o) { THORIN_R_CMP(CODE) default: unreachable(); } }
constexpr std::string_view op2str(Trait o) { switch (o) { THORIN_TRAIT(CODE) default: unreachable(); } }
constexpr std::string_view op2str(Conv  o) { switch (o) { THORIN_CONV (CODE) default: unreachable(); } }
constexpr std::string_view op2str(PE    o) { switch (o) { THORIN_PE   (CODE) default: unreachable(); } }
constexpr std::string_view op2str(Acc   o) { switch (o) { THORIN_ACC  (CODE) default: unreachable(); } }
#undef CODE

namespace AddrSpace {
    enum : nat_t {
        Generic  = 0,
        Global   = 1,
        Texture  = 2,
        Shared   = 3,
        Constant = 4,
    };
}

// This trick let's us count the number of elements in an enum class without tainting it with an extra "Num" field.
template<class T> constexpr size_t Num = size_t(-1);

#define CODE(T, o) + 1_s
constexpr size_t Num_Nodes = 0_s THORIN_NODE(CODE);
constexpr size_t Num_Tags  = 0_s THORIN_TAG (CODE);
template<> inline constexpr size_t Num<Bit  > = 0_s THORIN_BIT  (CODE);
template<> inline constexpr size_t Num<Shr  > = 0_s THORIN_SHR  (CODE);
template<> inline constexpr size_t Num<Wrap > = 0_s THORIN_WRAP (CODE);
template<> inline constexpr size_t Num<ROp  > = 0_s THORIN_R_OP (CODE);
template<> inline constexpr size_t Num<ICmp > = 0_s THORIN_I_CMP(CODE);
template<> inline constexpr size_t Num<RCmp > = 0_s THORIN_R_CMP(CODE);
template<> inline constexpr size_t Num<Trait> = 0_s THORIN_TRAIT(CODE);
template<> inline constexpr size_t Num<Conv > = 0_s THORIN_CONV (CODE);
template<> inline constexpr size_t Num<PE   > = 0_s THORIN_PE   (CODE);
template<> inline constexpr size_t Num<Acc  > = 0_s THORIN_ACC  (CODE);
#undef CODE

template<tag_t t> struct Tag2Enum_      { using type = tag_t; };
template<> struct Tag2Enum_<Tag::Bit  > { using type = Bit;     };
template<> struct Tag2Enum_<Tag::Shr  > { using type = Shr;     };
template<> struct Tag2Enum_<Tag::Wrap > { using type = Wrap;    };
template<> struct Tag2Enum_<Tag::ROp  > { using type = ROp;     };
template<> struct Tag2Enum_<Tag::ICmp > { using type = ICmp;    };
template<> struct Tag2Enum_<Tag::RCmp > { using type = RCmp;    };
template<> struct Tag2Enum_<Tag::Trait> { using type = Trait;   };
template<> struct Tag2Enum_<Tag::Conv > { using type = Conv;    };
template<> struct Tag2Enum_<Tag::PE   > { using type = PE;      };
template<> struct Tag2Enum_<Tag::Acc  > { using type = Acc;     };
template<tag_t t> using Tag2Enum = typename Tag2Enum_<t>::type;

// clang-format on
} // namespace thorin
