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
inline const App* type_ptr(Ref pointee, Ref addr_space) {
    World& w = pointee->world();
    return w.app(type_ptr(w), {pointee, addr_space})->as<App>();
}
inline const App* type_ptr(Ref pointee, nat_t addr_space = AddrSpace::Generic) {
    World& w = pointee->world();
    return type_ptr(pointee, w.lit_nat(addr_space));
}

/// Same as World::cn / World::pi but adds a World::type_mem-typed Var to each Pi.
inline const Pi* cn_mem(Ref dom) {
    World& w = dom->world();
    return w.cn({type_mem(w), dom});
}
inline const Pi* cn_mem_ret(Ref dom, Ref ret_dom) {
    World& w = dom->world();
    return w.cn({type_mem(w), dom, cn_mem(ret_dom)});
}
inline const Pi* pi_mem(Ref domain, Ref codomain) {
    World& w = domain->world();
    auto d   = w.sigma({type_mem(w), domain});
    return w.pi(d, w.sigma({type_mem(w), codomain}));
}
inline const Pi* fn_mem(Ref domain, Ref codomain) {
    World& w = domain->world();
    return w.cn({type_mem(w), domain, cn_mem(codomain)});
}

static inline Ref tuple_of_types(Ref t) {
    auto& world = t->world();
    if (auto sigma = t->isa<Sigma>()) return world.tuple(sigma->ops());
    if (auto arr = t->isa<Arr>()) return world.pack(arr->shape(), arr->body());
    return t;
}

inline Ref op_lea(Ref ptr, Ref index) {
    World& w                   = ptr->world();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();
    auto Ts                    = tuple_of_types(pointee);
    return w.app(w.app(w.ax<lea>(), {pointee->arity(), Ts, addr_space}), {ptr, index});
}

inline Ref op_lea_unsafe(Ref ptr, Ref i) {
    World& w = ptr->world();
    return op_lea(ptr, w.call(core::conv::u, force<Ptr>(ptr->type())->arg(0)->arity(), i));
}

inline Ref op_lea_unsafe(Ref ptr, u64 i) { return op_lea_unsafe(ptr, ptr->world().lit_int(64, i)); }

inline Ref op_remem(Ref mem) {
    World& w = mem->world();
    return w.app(w.ax<remem>(), mem);
}

inline Ref op_alloc(Ref type, Ref mem) {
    World& w = type->world();
    return w.app(w.app(w.ax<alloc>(), {type, w.lit_nat_0()}), mem);
}

inline Ref op_slot(Ref type, Ref mem) {
    World& w = type->world();
    return w.app(w.app(w.ax<slot>(), {type, w.lit_nat_0()}), {mem, w.lit_nat(w.curr_gid())});
}

inline Ref op_malloc(Ref type, Ref mem) {
    World& w  = type->world();
    auto size = w.call(core::trait::size, type);
    return w.app(w.app(w.ax<malloc>(), {type, w.lit_nat_0()}), {mem, size});
}

inline Ref op_mslot(Ref type, Ref mem, Ref id) {
    World& w  = type->world();
    auto size = w.call(core::trait::size, type);
    return w.app(w.app(w.ax<mslot>(), {type, w.lit_nat_0()}), {mem, size, id});
}

Ref op_malloc(Ref type, Ref mem);
Ref op_mslot(Ref type, Ref mem, Ref id);

/// Returns the (first) element of type mem::M from the given tuple.
static Ref mem_def(Ref def) {
    if (match<mem::M>(def->type())) return def;

    if (def->num_projs() > 1) {
        for (auto proj : def->projs())
            if (auto mem = mem_def(proj)) return mem;
    }

    return nullptr;
}

/// Returns the memory argument of a function if it has one.
inline Ref mem_var(Lam* lam) { return mem_def(lam->var()); }

/// Swapps the memory occurrences in the given def with the given memory.
inline Ref replace_mem(Ref mem, Ref arg) {
    // TODO: maybe use rebuild instead?
    if (arg->num_projs() > 1) {
        auto& w = mem->world();
        return w.tuple(DefArray(arg->num_projs(), [&](auto i) { return replace_mem(mem, arg->proj(i)); }));
    }

    if (match<mem::M>(arg->type())) return mem;

    return arg;
}

/// Removes recusively all occurences of mem from a type (sigma).
inline Ref strip_mem_ty(Ref def) {
    auto& world = def->world();

    if (auto sigma = def->isa<Sigma>()) {
        DefVec new_ops;
        for (auto op : sigma->ops())
            if (auto new_op = strip_mem_ty(op); new_op != world.sigma()) new_ops.push_back(new_op);

        return world.sigma(new_ops);
    } else if (match<mem::M>(def)) {
        return world.sigma();
    }

    return def;
}

/// Removes recusively all occurences of mem from a tuple.
/// Returns an empty tuple if applied with mem alone.
inline Ref strip_mem(Ref def) {
    auto& world = def->world();

    if (auto tuple = def->isa<Tuple>()) {
        DefVec new_ops;
        for (auto op : tuple->ops())
            if (auto new_op = strip_mem(op); new_op != world.tuple()) new_ops.push_back(new_op);

        return world.tuple(new_ops);
    } else if (match<mem::M>(def->type())) {
        return world.tuple();
    } else if (auto extract = def->isa<Extract>()) {
        // The case that this one element is a mem and should return () is handled above.
        if (extract->num_projs() == 1) return extract;

        DefVec new_ops;
        for (auto op : extract->projs())
            if (auto new_op = strip_mem(op); new_op != world.tuple()) new_ops.push_back(new_op);

        return world.tuple(new_ops);
    }

    return def;
}

} // namespace thorin::mem
