#pragma once

#include "thorin/axiom.h"
#include "thorin/lam.h"
#include "thorin/world.h"

#include "dialects/core/core.h"
#include "dialects/mem/autogen.h"

namespace thorin::mem {

namespace AddrSpace {
enum : nat_t {
    Generic  = 0,
    Global   = 1,
    Texture  = 2,
    Shared   = 3,
    Constant = 4,
};
}

// constructors
inline const Axiom* type_mem(World& w) { return w.ax<M>(); }

inline const Axiom* type_ptr(World& w) { return w.ax<Ptr>(); }
inline const App* type_ptr(Refer pointee, Refer addr_space, Refer dbg = {}) {
    World& w = pointee->world();
    return w.app(type_ptr(w), {pointee, addr_space}, dbg)->as<App>();
}
inline const App* type_ptr(Refer pointee, nat_t addr_space = AddrSpace::Generic, Refer dbg = {}) {
    World& w = pointee->world();
    return type_ptr(pointee, w.lit_nat(addr_space), dbg);
}

/// Same as World::cn / World::pi but adds a World::type_mem-typed Var to each Pi.
inline const Pi* cn_mem(Refer dom, Refer dbg = {}) {
    World& w = dom->world();
    return w.cn({type_mem(w), dom}, dbg);
}
inline const Pi* cn_mem_ret(Refer dom, Refer ret_dom, Refer dbg = {}) {
    World& w = dom->world();
    return w.cn({type_mem(w), dom, cn_mem(ret_dom)}, dbg);
}
inline const Pi* pi_mem(Refer domain, Refer codomain, Refer dbg = {}) {
    World& w = domain->world();
    auto d   = w.sigma({type_mem(w), domain});
    return w.pi(d, w.sigma({type_mem(w), codomain}), dbg);
}
inline const Pi* fn_mem(Refer domain, Refer codomain, Refer dbg = {}) {
    World& w = domain->world();
    return w.cn({type_mem(w), domain, cn_mem(codomain)}, dbg);
}

static inline const Def* tuple_of_types(Refer t) {
    auto& world = t->world();
    if (auto sigma = t->isa<Sigma>()) return world.tuple(sigma->ops());
    if (auto arr = t->isa<Arr>()) return world.pack(arr->shape(), arr->body());
    return t;
}

inline const Def* op_lea(Refer ptr, Refer index, Refer dbg = {}) {
    World& w                   = ptr->world();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();
    auto Ts                    = tuple_of_types(pointee);
    return w.app(w.app(w.ax<lea>(), {pointee->arity(), Ts, addr_space}), {ptr, index}, dbg);
}

inline const Def* op_lea_unsafe(Refer ptr, Refer i, Refer dbg = {}) {
    World& w      = ptr->world();
    auto safe_int = w.type_idx(force<Ptr>(ptr->type())->arg(0)->arity());
    return op_lea(ptr, core::op(core::conv::u2u, safe_int, i), dbg);
}

inline const Def* op_lea_unsafe(Refer ptr, u64 i, Refer dbg = {}) {
    World& w = ptr->world();
    return op_lea_unsafe(ptr, w.lit_idx(i), dbg);
}

inline const Def* op_alloc(Refer type, Refer mem, Refer dbg = {}) {
    World& w = type->world();
    return w.app(w.app(w.ax<alloc>(), {type, w.lit_nat_0()}), mem, dbg);
}

inline const Def* op_slot(Refer type, Refer mem, Refer dbg = {}) {
    World& w = type->world();
    return w.app(w.app(w.ax<slot>(), {type, w.lit_nat_0()}), {mem, w.lit_nat(w.curr_gid())}, dbg);
}

inline const Def* op_malloc(Refer type, Refer mem, Refer dbg) {
    World& w  = type->world();
    auto size = core::op(core::trait::size, type);
    return w.app(w.app(w.ax<malloc>(), {type, w.lit_nat_0()}), {mem, size}, dbg);
}

inline const Def* op_mslot(Refer type, Refer mem, Refer id, Refer dbg) {
    World& w  = type->world();
    auto size = core::op(core::trait::size, type);
    return w.app(w.app(w.ax<mslot>(), {type, w.lit_nat_0()}), {mem, size, id}, dbg);
}

inline const Def* mem_var(Lam* lam, Refer dbg = nullptr) {
    return match<M>(lam->var(0_s)->type()) ? lam->var(0, dbg) : nullptr;
}
} // namespace thorin::mem
