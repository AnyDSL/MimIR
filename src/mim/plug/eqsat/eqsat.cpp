#include "mim/plug/eqsat/eqsat.h"

#include <mim/plugin.h>

#include "mim/plug/eqsat/phase/egg_rewrite.h"
#include "mim/plug/eqsat/phase/eqsat_phase.h"
#include "mim/plug/eqsat/phase/slotted_rewrite.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Flags2Stages& stages) {
    Stage::hook<eqsat::eqsat_phase, eqsat::EqsatPhase>(stages);
    Stage::hook<eqsat::egg_rewrite, eqsat::EggRewrite>(stages);
    Stage::hook<eqsat::slotted_rewrite, eqsat::SlottedRewrite>(stages);
}

/// Registers normalizers as well as Phase%s and Pass%es for the Axm%s of this Plugin.
extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"eqsat", nullptr, reg_stages, nullptr}; }
