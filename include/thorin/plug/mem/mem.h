#pragma once

#include <thorin/axiom.h>
#include <thorin/lam.h>
#include <thorin/world.h>

#include <thorin/plug/core/core.h>

#include "thorin/plug/mem/autogen.h"

namespace thorin::plug::mem {

/// @name %%mem.M
///@{

inline Lam* mut_con(World& w) { return w.mut_con(w.annex<M>()); } ///< Yields `.con[%mem.M]`.

/// Yields `.con[%mem.M, dom]`.
inline Lam* mut_con(Ref dom) {
    World& w = dom->world();
    return w.mut_con({w.annex<M>(), dom});
}

/// Returns the (first) element of type mem::M from the given tuple.
inline Ref mem_def(Ref def) {
    if (match<mem::M>(def->type())) return def;
    if (def->type()->isa<Arr>()) return {}; // don't look into possibly gigantic arrays

    if (def->num_projs() > 1) {
        for (auto proj : def->projs())
            if (auto mem = mem_def(proj)) return mem;
    }

    return {};
}

/// Returns the memory argument of a function if it has one.
inline Ref mem_var(Lam* lam) { return mem_def(lam->var()); }

/// Swaps the memory occurrences in the given def with the given memory.
inline Ref replace_mem(Ref mem, Ref arg) {
    // TODO: maybe use rebuild instead?
    if (arg->num_projs() > 1) {
        auto& w = mem->world();
        return w.tuple(DefVec(arg->num_projs(), [&](auto i) { return replace_mem(mem, arg->proj(i)); }));
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

/// Recursively removes all occurrences of mem from a tuple.
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
///@}

/// @name %%mem.Ptr
///@{
enum class AddrSpace : nat_t {
    Generic  = 0,
    Global   = 1,
    Texture  = 2,
    Shared   = 3,
    Constant = 4,
};
///@}

/// @name %%mem.lea
///@{
inline Ref op_lea(Ref ptr, Ref index) {
    World& w                   = ptr->world();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();
    auto Ts                    = tuple_of_types(pointee);
    return w.app(w.app(w.annex<lea>(), {pointee->arity(), Ts, addr_space}), {ptr, index});
}

inline Ref op_lea_unsafe(Ref ptr, Ref i) {
    World& w = ptr->world();
    return op_lea(ptr, w.call(core::conv::u, force<Ptr>(ptr->type())->arg(0)->arity(), i));
}

inline Ref op_lea_unsafe(Ref ptr, u64 i) { return op_lea_unsafe(ptr, ptr->world().lit_i64(i)); }
///@}

/// @name %%mem.remem
///@{
inline Ref op_remem(Ref mem) {
    World& w = mem->world();
    return w.app(w.annex<remem>(), mem);
}
///@}

/// @name %%mem.alloc
///@{
inline Ref op_alloc(Ref type, Ref mem) {
    World& w = type->world();
    return w.app(w.app(w.annex<alloc>(), {type, w.lit_nat_0()}), mem);
}
///@}

/// @name %%mem.slot
///@{
inline Ref op_slot(Ref type, Ref mem) {
    World& w = type->world();
    return w.app(w.app(w.annex<slot>(), {type, w.lit_nat_0()}), {mem, w.lit_nat(w.curr_gid())});
}
///@}

/// @name %%mem.malloc
///@{
inline Ref op_malloc(Ref type, Ref mem) {
    World& w  = type->world();
    auto size = w.call(core::trait::size, type);
    return w.app(w.app(w.annex<malloc>(), {type, w.lit_nat_0()}), {mem, size});
}
///@}

/// @name %%mem.mslot
///@{
inline Ref op_mslot(Ref type, Ref mem, Ref id) {
    World& w  = type->world();
    auto size = w.call(core::trait::size, type);
    return w.app(w.app(w.annex<mslot>(), {type, w.lit_nat_0()}), {mem, size, id});
}
///@}

} // namespace thorin::plug::mem
