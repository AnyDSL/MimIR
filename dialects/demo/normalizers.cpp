#include "thorin/world.h"

#include "dialects/demo/demo.h"

namespace thorin::demo {

const Def* normalize_const(const Def*, const Def*, const Def* arg, const Def*) {
    auto& world = arg->world();
    return world.lit(world.type_idx(arg), 42);
}

THORIN_demo_NORMALIZER_IMPL

} // namespace thorin::demo
