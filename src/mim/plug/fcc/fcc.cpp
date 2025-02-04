#include "mim/plug/fcc/fcc.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

using namespace mim;

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axiom%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"fcc", [](Normalizers& normalizers) { plug::fcc::register_normalizers(normalizers); }, nullptr, nullptr};
}
