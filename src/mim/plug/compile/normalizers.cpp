#include <mim/driver.h>
#include <mim/world.h>

#include "mim/plug/compile/compile.h"

namespace mim::plug::compile {

/// `combined_phase (phase_list phase1 ... phasen)` -> `phases_to_phase n (phase1, ..., phasen)`
const Def* normalize_combined_phase(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();

    auto [axm, phases] = App::uncurry(arg);
    assert(axm->flags() == flags_t(Annex::Base<phase_list>));

    return world.call<phases_to_phase>(phases);
}

const Def* normalize_is_loaded(const Def*, const Def*, const Def* arg) {
    auto& world  = arg->world();
    auto& driver = world.driver();
    if (auto str = tuple2str(arg); !str.empty()) return world.lit_bool(driver.is_loaded(driver.sym(str)));

    return {};
}

MIM_compile_NORMALIZER_IMPL

} // namespace mim::plug::compile
