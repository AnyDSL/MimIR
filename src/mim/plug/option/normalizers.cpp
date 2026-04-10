#include "mim/world.h"

#include "mim/plug/option/option.h"

namespace mim::plug::option {

const Def* normalize_const(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_option_NORMALIZER_IMPL

} // namespace mim::plug::option
