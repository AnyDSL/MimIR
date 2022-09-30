#include "thorin/world.h"

#include "dialects/demo/demo.h"

namespace thorin::demo {

const Def* normalize_const(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    return world.lit_idx(world.type_idx(arg), 42);
}

THORIN_demo_NORMALIZER_IMPL

} // namespace thorin::demo
