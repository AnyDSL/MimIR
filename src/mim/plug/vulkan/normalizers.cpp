#include "mim/world.h"

#include "mim/plug/vulkan/vulkan.h"

namespace mim::plug::vulkan {

const Def* normalize_const(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_vulkan_NORMALIZER_IMPL

} // namespace mim::plug::vulkan
