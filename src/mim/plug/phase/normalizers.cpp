#include <mim/driver.h>
#include <mim/world.h>

#include "mim/plug/phase/phase.h"

namespace mim::plug::phase {

const Def* normalize_is_loaded(const Def*, const Def*, const Def* arg) {
    auto& world  = arg->world();
    auto& driver = world.driver();
    if (auto str = tuple2str(arg); !str.empty()) return world.lit_bool(driver.is_loaded(driver.sym(str)));

    return {};
}

MIM_phase_NORMALIZER_IMPL

} // namespace mim::plug::phase
