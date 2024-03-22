#include "thorin/world.h"

#include "thorin/plug/demo/demo.h"

namespace thorin::plug::demo {

Ref normalize_const(Ref type, Ref, Ref arg) {
    auto& world = type->world();
    return world.lit(world.Idx(arg), 42);
}

THORIN_demo_NORMALIZER_IMPL

} // namespace thorin::plug::demo
