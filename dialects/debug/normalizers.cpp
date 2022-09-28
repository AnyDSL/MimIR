#include "thorin/world.h"

#include "dialects/debug/debug.h"

namespace thorin::debug {

const Def* normalize_dbg(const Def*, const Def*, const Def* arg, const Def*) {
    debug_print(arg);
    return arg;
}

const Def* normalize_dbg_perm(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    debug_print(arg);
    return world.raw_app(callee, arg, dbg);
}

THORIN_debug_NORMALIZER_IMPL

} // namespace thorin::debug
