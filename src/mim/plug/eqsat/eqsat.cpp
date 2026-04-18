#include "mim/plug/eqsat/eqsat.h"

#include <mim/plugin.h>

#include "mim/plug/eqsat/phase/eqsat_rewrite.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Flags2Stages& stages) { Stage::hook<eqsat::eqsat_rewrite_phase, eqsat::EqsatRewrite>(stages); }

/// Registers normalizers as well as Phase%s and Pass%es for the Axm%s of this Plugin.
extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"eqsat", nullptr, reg_stages, nullptr}; }
