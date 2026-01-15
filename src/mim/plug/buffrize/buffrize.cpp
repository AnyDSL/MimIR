#include "mim/plug/buffrize/buffrize.h"

#include <mim/pass.h>
#include <mim/plugin.h>

#include "mim/plug/buffrize/phase/bufferize.h"

using namespace mim;
using namespace mim::plug;

static void reg_stages(Flags2Stages& stages) { Stage::hook<buffrize::buffrize_phase, buffrize::Bufferize>(stages); }

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axm%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"buffrize", nullptr, reg_stages, nullptr}; }
