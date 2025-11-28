#include "mim/world.h"

#include "mim/plug/spirv/spirv.h"

namespace mim::plug::spirv {

const Def* normalize_const(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_spirv_NORMALIZER_IMPL

} // namespace mim::plug::spirv
