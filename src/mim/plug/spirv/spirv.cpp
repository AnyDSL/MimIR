#include "mim/plug/spirv/spirv.h"

#include <mim/pass.h>
#include <mim/plugin.h>

#include "mim/plug/spirv/be/emit.h"

using namespace mim;
using namespace mim::plug;

/// Registers normalizers as well as Phase%s and Pass%es for the Axm%s of this Plugin.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"spirv", nullptr, nullptr, [](Backends& backends) { backends["spirv"] = &spirv::emit_asm; }};
}
