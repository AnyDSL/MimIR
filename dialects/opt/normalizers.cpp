#include "thorin/world.h"

#include "dialects/opt/opt.h"

namespace thorin::opt {

Ref normalize_is_loaded(Ref type, Ref callee, Ref arg) {
    auto& world = arg->world();
    world.DLOG("normalize is_loaded: {}", arg);
    return world.raw_app(type, callee, arg);
}

THORIN_opt_NORMALIZER_IMPL

} // namespace thorin::opt
