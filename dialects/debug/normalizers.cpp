#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/debug/debug.h"

namespace thorin::debug {

const Def* normalize_dbg(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    return arg;
}

THORIN_debug_NORMALIZER_IMPL

} // namespace thorin::debug
