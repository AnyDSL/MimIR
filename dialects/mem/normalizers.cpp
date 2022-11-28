#include "thorin/world.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

const Def* normalize_lea(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                = type->world();
    auto [ptr, index]          = arg->projs<2>();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();

    if (auto a = isa_lit(pointee->arity()); a && *a == 1) return ptr;
    // TODO

    return world.raw_app(type, callee, {ptr, index}, dbg);
}

const Def* normalize_load(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                = type->world();
    auto [mem, ptr]            = arg->projs<2>();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();

    if (ptr->isa<Bot>()) return world.tuple({mem, world.bot(type->as<Sigma>()->op(1))}, dbg);

    // loading an empty tuple can only result in an empty tuple
    if (auto sigma = pointee->isa<Sigma>(); sigma && sigma->num_ops() == 0)
        return world.tuple({mem, world.tuple(sigma->type(), {}, dbg)});

    return world.raw_app(type, callee, {mem, ptr}, dbg);
}

const Def* normalize_remem(const Def* type, const Def* callee, const Def* mem, const Def* dbg) {
    auto& world = type->world();

    // if (auto m = match<remem>(mem)) mem = m;
    return world.raw_app(type, callee, mem, dbg);
}

const Def* normalize_store(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world          = type->world();
    auto [mem, ptr, val] = arg->projs<3>();

    if (ptr->isa<Bot>() || val->isa<Bot>()) return mem;
    if (auto pack = val->isa<Pack>(); pack && pack->body()->isa<Bot>()) return mem;
    if (auto tuple = val->isa<Tuple>()) {
        if (std::ranges::all_of(tuple->ops(), [](const Def* op) { return op->isa<Bot>(); })) return mem;
    }

    return world.raw_app(type, callee, {mem, ptr, val}, dbg);
}

THORIN_mem_NORMALIZER_IMPL

} // namespace thorin::mem
