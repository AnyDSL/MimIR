#include "mim/plug/eqsat/eqsat.h"

#include <mim/plugin.h>

#include "mim/plug/eqsat/phase/egg_rewrite.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Flags2Stages& stages) {
    // clang-format off
    Stage::hook<eqsat::egg_rewrite_phase, eqsat::EggRewrite>(stages);
    // clang-format on
}

/// Registers normalizers as well as Phase%s and Pass%es for the Axm%s of this Plugin.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"eqsat", plug::eqsat::register_normalizers, reg_stages, nullptr};
}
