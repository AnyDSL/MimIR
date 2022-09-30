#include "thorin/world.h"

#include "dialects/demo/demo.h"

namespace thorin::demo {

const Def* normalize_const(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    return world.lit_idx(arg, 42);

    return world.raw_app(callee, arg, dbg);
}

THORIN_demo_NORMALIZER_IMPL

} // namespace thorin::demo
