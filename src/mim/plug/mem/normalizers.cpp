#include <mim/world.h>

#include <mim/pass/eta_red.h>

#include "mim/plug/mem/mem.h"
#include "mim/plug/mem/pass/alloc2malloc.h"
#include "mim/plug/mem/pass/copy_prop.h"
#include "mim/plug/mem/pass/remem_elim.h"
#include "mim/plug/mem/pass/reshape.h"
#include "mim/plug/mem/pass/ssa_constr.h"

namespace mim::plug::mem {

const Def* normalize_lea(const Def*, const Def*, const Def* arg) {
    auto [ptr, index]          = arg->projs<2>();
    auto [pointee, addr_space] = Axm::as<Ptr>(ptr->type())->args<2>();

    if (auto a = Lit::isa(pointee->arity()); a && *a == 1) return ptr;

    return {};
}

const Def* normalize_load(const Def* type, const Def*, const Def* arg) {
    auto& world                = type->world();
    auto [mem, ptr]            = arg->projs<2>();
    auto [pointee, addr_space] = Axm::as<Ptr>(ptr->type())->args<2>();

    if (ptr->isa<Bot>()) return world.tuple({mem, world.bot(type->as<Sigma>()->op(1))});

    // loading an empty tuple can only result in an empty tuple
    // TODO use common singleton magic here
    if (auto sigma = pointee->isa<Sigma>(); sigma && sigma->num_ops() == 0)
        return world.tuple({mem, world.tuple(sigma->type(), {})});

    return {};
}

const Def* normalize_store(const Def*, const Def*, const Def* arg) {
    auto [mem, ptr, val] = arg->projs<3>();

    if (ptr->isa<Bot>() || val->isa<Bot>()) return mem;
    if (auto pack = val->isa<Pack>(); pack && pack->body()->isa<Bot>()) return mem;
    if (auto tuple = val->isa<Tuple>()) {
        if (std::ranges::all_of(tuple->ops(), [](const Def* op) { return op->isa<Bot>(); })) return mem;
    }

    return {};
}

template<pass id>
const Def* normalize_pass(const Def* t, const Def* callee, const Def* arg) {
    switch (id) {
            // clang-format off
        case pass::alloc2malloc: return create<Alloc2Malloc>(id, t);
        case pass::remem_elim:   return create<RememElim   >(id, t);
        case pass::reshape:      return create<Reshape     >(id, t, Lit::get<bool   >(arg));
        case pass::ssa:          return create<SSAConstr   >(id, t, Lit::get<EtaExp*>(arg));
        // clang-format on
        case pass::copy_prop: {
            auto [bb_only, br] = App::uncurry_args<2>(callee);
            return create<CopyProp>(id, t, Lit::get<bool>(bb_only), Lit::get<BetaRed*>(br), Lit::get<EtaExp*>(arg));
        }
    }
}

MIM_mem_NORMALIZER_IMPL

} // namespace mim::plug::mem
