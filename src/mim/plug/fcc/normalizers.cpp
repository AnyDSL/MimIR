#include "mim/world.h"

#include "mim/plug/fcc/fcc.h"

namespace mim::plug::fcc {

Ref normalize_const(Ref type, Ref, Ref arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_fcc_NORMALIZER_IMPL

} // namespace mim::plug::fcc
