#include "mim/plug/sflow/sflow.h"

#include <mim/pass.h>
#include <mim/plugin.h>

#include "mim/plug/sflow/cfg.h"

using namespace mim;

static void emit_cfg(World& world, std::ostream& os) { plug::sflow::nest_cfg_dot(Nest(world), os); }

/// Registers normalizers as well as Phase%s and Pass%es for the Axm%s of this Plugin.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"sflow", plug::sflow::register_normalizers, nullptr,
            [](Backends& backends) { backends["cfg"] = &emit_cfg; }};
}
