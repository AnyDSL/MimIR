#include "thorin/world.h"

#include "dialects/opt/opt.h"

namespace thorin::opt {

const Def* normalize_is_loaded(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = arg->world();
    world.DLOG("normalize is_loaded: {}", arg);
    return world.raw_app(type, callee, arg, dbg);
}

THORIN_opt_NORMALIZER_IMPL

} // namespace thorin::opt
