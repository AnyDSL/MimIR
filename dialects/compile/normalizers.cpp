#include "dialects/compile/compile.h"

namespace thorin::compile {

const Def* normalize_pass_phase(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    // pass_phase (pass_list pass1 ... passn)
    // -> passes_to_phase n (pass1, ..., passn)

    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_combined_phase(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    // combined_phase (phase_list phase1 ... phasen)
    // -> phases_to_phase n (phase1, ..., phasen)

    return world.raw_app(callee, arg, dbg);
}

THORIN_compile_NORMALIZER_IMPL

} // namespace thorin::compile
