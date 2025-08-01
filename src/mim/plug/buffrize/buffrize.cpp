#include "mim/plug/buffrize/buffrize.h"


#include <mim/pass/pass.h>
#include <mim/pass/pipelinebuilder.h>
#include <mim/plugin.h>

#include <mim/pass/pass.h>

using namespace mim;
using namespace mim::plug;

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axm%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"buffrize", nullptr,
            [](Passes& passes) { register_phase<buffrize::buffrize_phase, buffrize::Bufferize>(passes); },
            nullptr};
}
