#pragma once

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/core/autogen.h"

namespace thorin::core {

/// @name Mode
///@{
/// What should happen if Idx arithmetic overflows?
enum class Mode : nat_t {
    none = 0,      ///< Wrap around.
    nsw  = 1 << 0, ///< No Signed Wrap around.
    nuw  = 1 << 1, ///< No Unsigned Wrap around.
};

THORIN_ENUM_OPERATORS(Mode)

/// Give Mode as thorin::math::Mode, thorin::nat_t or Ref.
using VMode = std::variant<Mode, nat_t, Ref>;

/// thorin::core::VMode -> Ref.
inline Ref mode(World& w, VMode m) {
    if (auto def = std::get_if<Ref>(&m)) return *def;
    if (auto nat = std::get_if<nat_t>(&m)) return w.lit_nat(*nat);
    return w.lit_nat((nat_t)std::get<Mode>(m));
}
///@}

/// @name %%core.trait
///@{
inline Ref op(trait o, Ref type) {
    World& w = type->world();
    return w.app(w.annex(o), type);
}
///@}

/// @name %%core.pe
///@{
inline Ref op(pe o, Ref def) {
    World& w = def->world();
    return w.app(w.app(w.annex(o), def->type()), def);
}
///@}

/// @name %%core.bit2
///@{

/// Use like this: `a op b = tab[a][b]`
constexpr std::array<std::array<u64, 2>, 2> make_truth_table(bit2 id) {
    return {
        {{sub_t(id) & sub_t(0b0001) ? u64(-1) : 0, sub_t(id) & sub_t(0b0100) ? u64(-1) : 0},
         {sub_t(id) & sub_t(0b0010) ? u64(-1) : 0, sub_t(id) & sub_t(0b1000) ? u64(-1) : 0}}
    };
}
///@}

/// @name extract_unsafe
///@{
inline Ref extract_unsafe(Ref d, Ref i) {
    World& w = d->world();
    return w.extract(d, w.call(conv::u, d->unfold_type()->arity(), i));
}
inline Ref extract_unsafe(Ref d, u64 i) {
    World& w = d->world();
    return extract_unsafe(d, w.lit_idx(0_u64, i));
}
///@}

/// @name insert_unsafe
///@{
inline Ref insert_unsafe(Ref d, Ref i, Ref val) {
    World& w = d->world();
    return w.insert(d, w.call(conv::u, d->unfold_type()->arity(), i), val);
}
inline Ref insert_unsafe(Ref d, u64 i, Ref val) {
    World& w = d->world();
    return insert_unsafe(d, w.lit_idx(0_u64, i), val);
}
///@}

/// @name Convert TBound to Sigma
///@{
/// This is WIP.
template<bool up> const Sigma* convert(const TBound<up>* b);

inline const Sigma* convert(const Bound* b) { return b->isa<Join>() ? convert(b->as<Join>()) : convert(b->as<Meet>()); }
///@}

} // namespace thorin::core

namespace thorin {

/// @name is_commutative/is_associative
///@{
// clang-format off
constexpr bool is_commutative(core::nat  id) { return id == core::nat ::add || id == core::nat ::mul; }
constexpr bool is_commutative(core::ncmp id) { return id == core::ncmp::  e || id == core::ncmp:: ne; }
constexpr bool is_commutative(core::wrap id) { return id == core::wrap::add || id == core::wrap::mul; }
constexpr bool is_commutative(core::icmp id) { return id == core::icmp::  e || id == core::icmp:: ne; }
// clang-format off

constexpr bool is_commutative(core::bit2 id) {
    auto tab = make_truth_table(id);
    return tab[0][1] == tab[1][0];
}

constexpr bool is_associative(core::bit2 id) {
    switch (id) {
        case core::bit2::t:
        case core::bit2::xor_:
        case core::bit2::and_:
        case core::bit2::nxor:
        case core::bit2::fst:
        case core::bit2::snd:
        case core::bit2::or_:
        case core::bit2::f: return true;
        default: return false;
    }
}
///@}

} // namespace thorin
