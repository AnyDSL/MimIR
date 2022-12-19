#include "dialects/compile/autogen.h"
#include "dialects/compile/compile.h"

namespace thorin::compile {

// `pass_phase (pass_list pass1 ... passn)` -> `passes_to_phase n (pass1, ..., passn)`
const Def* normalize_pass_phase(const Def* type, const Def*, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    auto [ax, _] = collect_args(arg);
    if (ax->flags() != flags_t(Axiom::Base<pass_list>)) {
        // return world.raw_app(type, callee, arg, dbg);
        // TODO: remove when normalizers are fixed
        if (ax->flags() == flags_t(Axiom::Base<combine_pass_list>)) {
            auto arg_cpl = arg->as<App>();
            arg = normalize_combine_pass_list(arg_cpl->type(), arg_cpl->callee(), arg_cpl->arg(), arg_cpl->dbg());
        } else {
            world.ELOG("pass_phase expects a pass_list as argument but got {}", arg);
        }
    }

    auto [f_ax, pass_list_defs] = collect_args(arg);
    assert(f_ax->flags() == flags_t(Axiom::Base<pass_list>));
    auto n = pass_list_defs.size();

    return world.app(world.app(world.ax<passes_to_phase>(), world.lit_nat(n), dbg), pass_list_defs, dbg);
}

/// `combined_phase (phase_list phase1 ... phasen)` -> `phases_to_phase n (phase1, ..., phasen)`
const Def* normalize_combined_phase(const Def* type, const Def*, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    auto [ax, phase_list_defs] = collect_args(arg);
    assert(ax->flags() == flags_t(Axiom::Base<phase_list>));
    auto n = phase_list_defs.size();

    return world.app(world.app(world.ax<phases_to_phase>(), world.lit_nat(n), dbg), phase_list_defs, dbg);
}

/// `single_pass_phase pass` -> `passes_to_phase 1 pass`
const Def* normalize_single_pass_phase(const Def* type, const Def*, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return world.app(world.app(world.ax<passes_to_phase>(), world.lit_nat_1(), dbg), arg, dbg);
}

/// `combine_pass_list K (pass_list pass11 ... pass1N) ... (pass_list passK1 ... passKM) = pass_list pass11 ... p1N ...
/// passK1 ... passKM`
const Def* normalize_combine_pass_list(const Def* type, const Def*, const Def* arg, const Def* dbg) {
    auto& world     = type->world();
    auto pass_lists = arg->projs();
    DefVec passes;

    for (auto pass_list_def : pass_lists) {
        auto [ax, pass_list_defs] = collect_args(pass_list_def);
        assert(ax->flags() == flags_t(Axiom::Base<pass_list>));
        passes.insert(passes.end(), pass_list_defs.begin(), pass_list_defs.end());
    }
    const Def* app_list = world.ax<pass_list>();
    for (auto pass : passes) app_list = world.app(app_list, pass, dbg);
    return app_list;
}

THORIN_compile_NORMALIZER_IMPL

} // namespace thorin::compile
