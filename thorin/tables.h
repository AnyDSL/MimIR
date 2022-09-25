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

namespace Node {
#define CODE(node, name) node,
enum : node_t { THORIN_NODE(CODE) Max };
#undef CODE
}

// This trick let's us count the number of elements in an enum class without tainting it with an extra "Num" field.
template<class T> constexpr size_t Num = size_t(-1);

template<tag_t t> struct Tag2Enum_      { using type = tag_t; };
template<tag_t t> using Tag2Enum = typename Tag2Enum_<t>::type;

// clang-format on
} // namespace thorin
