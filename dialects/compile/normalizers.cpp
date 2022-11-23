#include "dialects/compile/compile.h"

namespace thorin::compile {

// `pass_phase (pass_list pass1 ... passn)` -> `passes_to_phase n (pass1, ..., passn)`
const Def* normalize_pass_phase(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    auto [ax, pass_list_defs] = collect_args(arg);
    assert(ax->flags() == flags_t(Axiom::Base<pass_list>));
    auto n = pass_list_defs.size();

    return world.raw_app(world.raw_app(world.ax<passes_to_phase>(), world.lit_nat(n)), world.tuple(pass_list_defs));
}

/// `combined_phase (phase_list phase1 ... phasen)` -> `phases_to_phase n (phase1, ..., phasen)`
const Def* normalize_combined_phase(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    auto [ax, phase_list_defs] = collect_args(arg);
    assert(ax->flags() == flags_t(Axiom::Base<phase_list>));
    auto n = phase_list_defs.size();

    return world.raw_app(world.raw_app(world.ax<phases_to_phase>(), world.lit_nat(n)), world.tuple(phase_list_defs));
}

/// `single_pass_phase pass` -> `passes_to_phase 1 pass`
const Def* normalize_single_pass_phase(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return world.raw_app(world.raw_app(world.ax<passes_to_phase>(), world.lit_nat_1()), arg);
}

THORIN_compile_NORMALIZER_IMPL

} // namespace thorin::compile
