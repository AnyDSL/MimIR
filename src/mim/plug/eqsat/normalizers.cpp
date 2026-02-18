#include "mim/world.h"

#include "mim/plug/eqsat/eqsat.h"

namespace mim::plug::eqsat {

const Def* normalize_const(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_eqsat_NORMALIZER_IMPL

} // namespace mim::plug::eqsat
