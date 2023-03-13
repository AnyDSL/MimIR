#include "thorin/world.h"

#include "dialects/demo/demo.h"

namespace thorin::demo {

Ref normalize_const(Ref type, Ref, Ref arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

THORIN_demo_NORMALIZER_IMPL

} // namespace thorin::demo
