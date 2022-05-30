#ifndef DIALECTS_MEM_MEM_H
#define DIALECTS_MEM_MEM_H

#include "thorin/axiom.h"
#include "thorin/lam.h"
#include "thorin/world.h"

#include "dialects/mem.h"

namespace thorin::mem {

// constructors
inline const Axiom* type_mem(World& w) { return w.ax(Tag::mem_M); }

inline const Axiom* type_ptr(World& w) { return w.ax(Tag::mem_Ptr); }
inline const App* type_ptr(const Def* pointee, const Def* addr_space, const Def* dbg = {}) {
    World& w = pointee->world();
    return w.app(type_ptr(w), {pointee, addr_space}, dbg)->as<App>();
}
inline const App* type_ptr(const Def* pointee, nat_t addr_space = AddrSpace::Generic, const Def* dbg = {}) {
    World& w = pointee->world();
    return type_ptr(pointee, w.lit_nat(addr_space), dbg);
}

/// Same as World::cn / World::pi but adds a World::type_mem-typed Var to each Pi.
inline const Pi* cn_mem(const Def* dom, const Def* dbg = {}) {
    World& w = dom->world();
    return w.cn({type_mem(w), dom}, dbg);
}
inline const Pi* cn_mem_ret(const Def* dom, const Def* ret_dom, const Def* dbg = {}) {
    World& w = dom->world();
    return w.cn({type_mem(w), dom, cn_mem(ret_dom)}, dbg);
}
inline const Pi* pi_mem(const Def* domain, const Def* codomain, const Def* dbg = {}) {
    World& w = domain->world();
    auto d   = w.sigma({type_mem(w), domain});
    return w.pi(d, w.sigma({type_mem(w), codomain}), dbg);
}
inline const Pi* fn_mem(const Def* domain, const Def* codomain, const Def* dbg = {}) {
    World& w = domain->world();
    return w.cn({type_mem(w), domain, cn_mem(codomain)}, dbg);
}

static inline const Def* tuple_of_types(const Def* t) {
    auto& world = t->world();
    if (auto sigma = t->isa<Sigma>()) return world.tuple(sigma->ops());
    if (auto arr = t->isa<Arr>()) return world.pack(arr->shape(), arr->body());
    return t;
}

inline const Def* op_lea(const Def* ptr, const Def* index, const Def* dbg = {}) {
    World& w                   = ptr->world();
    auto [pointee, addr_space] = mem::as<Tag::mem_Ptr>(ptr->type())->args<2>();
    auto Ts                    = tuple_of_types(pointee);
    return w.app(w.app(w.ax(Tag::mem_lea), {pointee->arity(), Ts, addr_space}), {ptr, index}, dbg);
}

inline const Def* op_lea_unsafe(const Def* ptr, const Def* i, const Def* dbg = {}) {
    World& w      = ptr->world();
    auto safe_int = w.type_int(mem::as<Tag::mem_Ptr>(ptr->type())->arg(0)->arity());
    return op_lea(ptr, w.op(Conv::u2u, safe_int, i), dbg);
}

inline const Def* op_lea_unsafe(const Def* ptr, u64 i, const Def* dbg = {}) {
    World& w = ptr->world();
    return op_lea_unsafe(ptr, w.lit_int(i), dbg);
}

inline const Def* op_load(const Def* mem, const Def* ptr, const Def* dbg = {}) {
    World& w    = mem->world();
    auto [T, a] = mem::as<Tag::mem_Ptr>(ptr->type())->args<2>();
    return w.app(w.app(w.ax(Tag::mem_load), {T, a}), {mem, ptr}, dbg);
}

inline const Def* op_store(const Def* mem, const Def* ptr, const Def* val, const Def* dbg = {}) {
    World& w    = mem->world();
    auto [T, a] = mem::as<Tag::mem_Ptr>(ptr->type())->args<2>();
    return w.app(w.app(w.ax(Tag::mem_store), {T, a}), {mem, ptr, val}, dbg);
}

inline const Def* mem_var(Lam* lam, const Def* dbg = nullptr) {
    return mem::isa<Tag::mem_M>(lam->var(0_s)->type()) ? lam->var(0, dbg) : nullptr;
}
} // namespace thorin::mem

#endif