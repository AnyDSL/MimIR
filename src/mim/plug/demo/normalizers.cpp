#include "mim/plug/demo/demo.h"
#include "mim/world.h"

namespace mim::plug::demo {

Ref normalize_const(Ref type, Ref, Ref arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_demo_NORMALIZER_IMPL

} // namespace mim::plug::demo
