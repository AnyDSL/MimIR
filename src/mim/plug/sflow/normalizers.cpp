#include "mim/world.h"

#include "mim/plug/sflow/sflow.h"

namespace mim::plug::sflow {

const Def* normalize_const(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

const Def* normalize_branch(const Def* type, const Def* callee, const Def* args) {
    auto& world                            = type->world();
    auto [n, Ts, merge, tuple, index, arg] = App::uncurry_args<6>(callee);

    if (Lit::as(n) == 1) return world.app(world.extract(tuple, index), arg);

    return world.raw_app(type, callee, args);
}

MIM_sflow_NORMALIZER_IMPL

} // namespace mim::plug::sflow
