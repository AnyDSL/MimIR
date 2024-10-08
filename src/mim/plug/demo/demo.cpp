#include "mim/plug/demo/demo.h"

#include <mim/pass/pass.h>
#include <mim/plugin.h>

using namespace mim;

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axiom%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"demo", [](Normalizers& normalizers) { plug::demo::register_normalizers(normalizers); }, nullptr, nullptr};
}
