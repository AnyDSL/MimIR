#include "mim/plug/vec/vec.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

using namespace mim;

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axm%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"vec", [](Normalizers& normalizers) { plug::vec::register_normalizers(normalizers); }, nullptr, nullptr};
}
