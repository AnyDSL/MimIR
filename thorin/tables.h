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

#define THORIN_TAG(m) m(PE, pe)

/// Partial Evaluation related operations
#define THORIN_PE(m) m(PE, hlt) m(PE, known) m(PE, run)

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
enum class PE     : sub_t { THORIN_PE   (CODE) };
#undef CODE

#define CODE(T, o) case T::o: return #T "_" #o;
constexpr std::string_view op2str(PE    o) { switch (o) { THORIN_PE   (CODE) default: unreachable(); } }
#undef CODE

// This trick let's us count the number of elements in an enum class without tainting it with an extra "Num" field.
template<class T> constexpr size_t Num = size_t(-1);

#define CODE(T, o) + 1_s
constexpr size_t Num_Nodes = 0_s THORIN_NODE(CODE);
constexpr size_t Num_Tags  = 0_s THORIN_TAG (CODE);
template<> inline constexpr size_t Num<PE   > = 0_s THORIN_PE   (CODE);
#undef CODE

template<tag_t t> struct Tag2Enum_      { using type = tag_t; };
template<> struct Tag2Enum_<Tag::PE   > { using type = PE;      };
template<tag_t t> using Tag2Enum = typename Tag2Enum_<t>::type;

// clang-format on
} // namespace thorin
