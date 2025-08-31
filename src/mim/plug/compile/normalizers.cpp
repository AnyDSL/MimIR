#include <mim/driver.h>
#include <mim/world.h>

#include "mim/plug/compile/compile.h"

namespace mim::plug::compile {

// `pass_phase (pass_list pass1 ... passn)` -> `passes_to_phase n (pass1, ..., passn)`
const Def* normalize_pass_phase(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();

    auto axm = App::uncurry_callee(arg)->as<Axm>(); // TODO or use isa?
    if (axm->flags() != flags_t(Annex::Base<pass_list>)) {
        // TODO: remove when normalizers are fixed
        if (axm->flags() == flags_t(Annex::Base<combine_pass_list>)) {
            auto arg_cpl = arg->as<App>();
            arg          = normalize_combine_pass_list(arg_cpl->type(), arg_cpl->callee(), arg_cpl->arg());
        } else {
            world.ELOG("pass_phase expects a pass_list as argument but got {}", arg);
        }
    }

    auto [f_axm, passes] = App::uncurry(arg);
    assert(f_axm->flags() == flags_t(Annex::Base<pass_list>));

    return world.call<passes_to_phase>(passes);
}

/// `combined_phase (phase_list phase1 ... phasen)` -> `phases_to_phase n (phase1, ..., phasen)`
const Def* normalize_combined_phase(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();

    auto [axm, phases] = App::uncurry(arg);
    assert(axm->flags() == flags_t(Annex::Base<phase_list>));

    return world.call<phases_to_phase>(phases);
}

/// `combine_pass_list K (pass_list pass11 ... pass1N) ... (pass_list passK1 ... passKM) = pass_list pass11 ... p1N ...
/// passK1 ... passKM`
const Def* normalize_combine_pass_list(const Def* type, const Def*, const Def* arg) {
    auto& world     = type->world();
    auto pass_lists = arg->projs();
    DefVec passes;

    for (auto pass_list_def : pass_lists) {
        auto [axm, pass_list_defs] = App::uncurry(pass_list_def);
        assert(axm->flags() == flags_t(Annex::Base<pass_list>));
        passes.insert(passes.end(), pass_list_defs.begin(), pass_list_defs.end());
    }

    auto app_list = world.annex<pass_list>();
    for (auto pass : passes)
        app_list = world.app(app_list, pass);

    return app_list;
}

const Def* normalize_is_loaded(const Def*, const Def*, const Def* arg) {
    auto& world  = arg->world();
    auto& driver = world.driver();
    if (auto str = tuple2str(arg); !str.empty()) return world.lit_bool(driver.is_loaded(driver.sym(str)));

    return {};
}

MIM_compile_NORMALIZER_IMPL

} // namespace mim::plug::compile
