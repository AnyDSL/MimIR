#include "mim/world.h"

#include "mim/plug/sflow/sflow.h"

namespace mim::plug::sflow {

const Def* normalize_const(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_sflow_NORMALIZER_IMPL

} // namespace mim::plug::sflow
