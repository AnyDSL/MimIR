#include "mim/world.h"

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem {

Ref normalize_lea(Ref type, Ref callee, Ref arg) {
    auto& world                = type->world();
    auto [ptr, index]          = arg->projs<2>();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();

    if (auto a = Lit::isa(pointee->arity()); a && *a == 1) return ptr;
    // TODO

    return world.raw_app(type, callee, {ptr, index});
}

Ref normalize_load(Ref type, Ref callee, Ref arg) {
    auto& world                = type->world();
    auto [mem, ptr]            = arg->projs<2>();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();

    if (ptr->isa<Bot>()) return world.tuple({mem, world.bot(type->as<Sigma>()->op(1))});

    // loading an empty tuple can only result in an empty tuple
    if (auto sigma = pointee->isa<Sigma>(); sigma && sigma->num_ops() == 0)
        return world.tuple({mem, world.tuple(sigma->type(), {})});

    return world.raw_app(type, callee, {mem, ptr});
}

Ref normalize_remem(Ref type, Ref callee, Ref mem) {
    auto& world = type->world();

    // if (auto m = match<remem>(mem)) mem = m;
    return world.raw_app(type, callee, mem);
}

Ref normalize_store(Ref type, Ref callee, Ref arg) {
    auto& world          = type->world();
    auto [mem, ptr, val] = arg->projs<3>();

    if (ptr->isa<Bot>() || val->isa<Bot>()) return mem;
    if (auto pack = val->isa<Pack>(); pack && pack->body()->isa<Bot>()) return mem;
    if (auto tuple = val->isa<Tuple>()) {
        if (std::ranges::all_of(tuple->ops(), [](Ref op) { return op->isa<Bot>(); })) return mem;
    }

    return world.raw_app(type, callee, {mem, ptr, val});
}

MIM_mem_NORMALIZER_IMPL

} // namespace mim::plug::mem
