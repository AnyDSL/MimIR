#pragma once

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/core/autogen.h"

namespace thorin::core {

enum Mode : nat_t {
    none = 0,
    nsw  = 1 << 0,
    nuw  = 1 << 1,
};

using VMode = std::variant<Mode, nat_t, const Def*>;

inline const Def* mode(World& w, VMode m) {
    if (auto def = std::get_if<const Def*>(&m)) return *def;
    if (auto nat = std::get_if<nat_t>(&m)) return w.lit_nat(*nat);
    return w.lit_nat(std::get<Mode>(m));
}

/// @name fn - these guys yield the final function to be invoked for the various operations
///@{
inline const Def* fn_bitcast(const Def* dst_t, const Def* src_t, const Def* dbg = {}) {
    World& w = dst_t->world();
    return w.app(w.ax<bitcast>(), {dst_t, src_t}, dbg);
}
///@}

/// @name op - these guys build the final function application for the various operations
///@{
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
    return w.extract(d, w.call(conv::u, d->unfold_type()->arity(), i), dbg);
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
    return w.insert(d, w.call(conv::u, d->unfold_type()->arity(), i), val, dbg);
}
inline const Def* insert_unsafe(const Def* d, u64 i, const Def* val, const Def* dbg = {}) {
    World& w = d->world();
    return insert_unsafe(d, w.lit_idx(0_u64, i), val, dbg);
}
///@}

/// @name wrappers for unary operations
///@{
inline const Def* op_wminus(VMode m, const Def* a, const Def* dbg = {}) {
    World& w = a->world();
    auto s   = as_lit(w.iinfer(a));
    return w.dcall(dbg, wrap::sub, mode(w, m), Defs{w.lit_idx(s, 0), a});
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

namespace thorin {

/// @name is_commutative/is_associative
///@{
// clang-format off
constexpr bool is_commutative(core::nop    ) { return true; }
constexpr bool is_commutative(core::wrap id) { return id == core::wrap::add || id == core::wrap::mul; }
constexpr bool is_commutative(core::ncmp id) { return id == core::ncmp::  e || id == core::ncmp:: ne; }
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
