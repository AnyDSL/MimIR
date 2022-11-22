#include "dialects/compile/compile.h"

namespace thorin::compile {

const Def* normalize_pass_phase(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    return world.raw_app(callee, arg, dbg);
}

THORIN_compile_NORMALIZER_IMPL

} // namespace thorin::compile
