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
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();
    auto Ts                    = tuple_of_types(pointee);
    return w.app(w.app(w.ax<lea>(), {pointee->arity(), Ts, addr_space}), {ptr, index}, dbg);
}

inline const Def* op_lea_unsafe(const Def* ptr, const Def* i, const Def* dbg = {}) {
    World& w = ptr->world();
    return op_lea(ptr, w.call(core::conv::u, force<Ptr>(ptr->type())->arg(0)->arity(), i), dbg);
}

inline const Def* op_lea_unsafe(const Def* ptr, u64 i, const Def* dbg = {}) {
    World& w = ptr->world();
    return op_lea_unsafe(ptr, w.lit_idx(i), dbg);
}

inline const Def* op_remem(const Def* mem, const Def* dbg = {}) {
    World& w = mem->world();
    return w.app(w.ax<remem>(), mem, dbg);
}

inline const Def* op_alloc(const Def* type, const Def* mem, const Def* dbg = {}) {
    World& w = type->world();
    return w.app(w.app(w.ax<alloc>(), {type, w.lit_nat_0()}), mem, dbg);
}

inline const Def* op_slot(const Def* type, const Def* mem, const Def* dbg = {}) {
    World& w = type->world();
    return w.app(w.app(w.ax<slot>(), {type, w.lit_nat_0()}), {mem, w.lit_nat(w.curr_gid())}, dbg);
}

inline const Def* op_malloc(const Def* type, const Def* mem, const Def* dbg) {
    World& w  = type->world();
    auto size = w.dcall(dbg, core::trait::size, type);
    return w.app(w.app(w.ax<malloc>(), {type, w.lit_nat_0()}), {mem, size}, dbg);
}

inline const Def* op_mslot(const Def* type, const Def* mem, const Def* id, const Def* dbg) {
    World& w  = type->world();
    auto size = w.dcall(dbg, core::trait::size, type);
    return w.app(w.app(w.ax<mslot>(), {type, w.lit_nat_0()}), {mem, size, id}, dbg);
}

const Def* op_malloc(const Def* type, const Def* mem, const Def* dbg = {});
const Def* op_mslot(const Def* type, const Def* mem, const Def* id, const Def* dbg = {});

/// Returns the (first) element of type mem::M from the given tuple.
static const Def* mem_def(const Def* def, const Def* dbg = nullptr) {
    if (match<mem::M>(def->type())) { return def; }

    if (def->num_projs() > 1) {
        for (auto proj : def->projs()) {
            if (auto mem = mem_def(proj)) { return mem; }
        }
    }

    return nullptr;
}

/// Returns the memory argument of a function if it has one.
inline const Def* mem_var(Lam* lam, const Def* dbg = nullptr) { return mem_def(lam->var(), dbg); }

/// Swapps the memory occurrences in the given def with the given memory.
inline const Def* replace_mem(const Def* mem, const Def* arg) {
    // TODO: maybe use rebuild instead?
    if (arg->num_projs() > 1) {
        auto& w = mem->world();
        return w.tuple(DefArray(arg->num_projs(), [&](auto i) { return replace_mem(mem, arg->proj(i)); }));
    }

    if (match<mem::M>(arg->type())) { return mem; }

    return arg;
}

/// Removes recusively all occurences of mem from a type (sigma).
static const Def* strip_mem_ty(const Def* def) {
    auto& world = def->world();

    if (auto sigma = def->isa<Sigma>()) {
        DefVec newOps;
        for (auto op : sigma->ops()) {
            auto newOp = strip_mem_ty(op);
            if (newOp != world.sigma()) { newOps.push_back(newOp); }
        }

        return world.sigma(newOps);
    } else if (match<mem::M>(def)) {
        return world.sigma();
    }

    return def;
}

/// Removes recusively all occurences of mem from a tuple.
/// Returns an empty tuple if applied with mem alone.
static const Def* strip_mem(const Def* def) {
    auto& world = def->world();

    if (auto tuple = def->isa<Tuple>()) {
        DefVec newOps;
        for (auto op : tuple->ops()) {
            auto newOp = strip_mem(op);
            if (newOp != world.tuple()) { newOps.push_back(newOp); }
        }

        return world.tuple(newOps);
    } else if (match<mem::M>(def->type())) {
        return world.tuple();
    } else if (auto extract = def->isa<Extract>()) {
        // The case that this one element is a mem and should return () is handled above.
        if (extract->num_projs() == 1) { return extract; }

        DefVec newOps;
        for (auto op : extract->projs()) {
            auto newOp = strip_mem(op);
            if (newOp != world.tuple()) { newOps.push_back(newOp); }
        }

        return world.tuple(newOps);
    }

    return def;
}

} // namespace thorin::mem
