#include "mim/plug/compile/autogen.h"
#include "mim/plug/compile/compile.h"

namespace mim::plug::compile {

// `pass_phase (pass_list pass1 ... passn)` -> `passes_to_phase n (pass1, ..., passn)`
const Def* normalize_pass_phase(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();

    auto [ax, _] = collect_args(arg);
    if (ax->flags() != flags_t(Annex::Base<pass_list>)) {
        // TODO: remove when normalizers are fixed
        if (ax->flags() == flags_t(Annex::Base<combine_pass_list>)) {
            auto arg_cpl = arg->as<App>();
            arg          = normalize_combine_pass_list(arg_cpl->type(), arg_cpl->callee(), arg_cpl->arg());
        } else {
            world.ELOG("pass_phase expects a pass_list as argument but got {}", arg);
        }
    }

    auto [f_ax, pass_list_defs] = collect_args(arg);
    assert(f_ax->flags() == flags_t(Annex::Base<pass_list>));

    return world.call<passes_to_phase>(pass_list_defs);
}

/// `combined_phase (phase_list phase1 ... phasen)` -> `phases_to_phase n (phase1, ..., phasen)`
const Def* normalize_combined_phase(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();

    auto [ax, phase_list_defs] = collect_args(arg);
    assert(ax->flags() == flags_t(Annex::Base<phase_list>));

    return world.call<phases_to_phase>(phase_list_defs);
}

/// `single_pass_phase pass` -> `passes_to_phase pass`
const Def* normalize_single_pass_phase(const Def* type, const Def*, const Def* arg) {
    return type->world().call<passes_to_phase>(arg);
}

/// `combine_pass_list K (pass_list pass11 ... pass1N) ... (pass_list passK1 ... passKM) = pass_list pass11 ... p1N ...
/// passK1 ... passKM`
const Def* normalize_combine_pass_list(const Def* type, const Def*, const Def* arg) {
    auto& world     = type->world();
    auto pass_lists = arg->projs();
    DefVec passes;

    for (auto pass_list_def : pass_lists) {
        auto [ax, pass_list_defs] = collect_args(pass_list_def);
        assert(ax->flags() == flags_t(Annex::Base<pass_list>));
        passes.insert(passes.end(), pass_list_defs.begin(), pass_list_defs.end());
    }
    const Def* app_list = world.annex<pass_list>();
    for (auto pass : passes) app_list = world.app(app_list, pass);
    return app_list;
}

MIM_compile_NORMALIZER_IMPL

} // namespace mim::plug::compile
