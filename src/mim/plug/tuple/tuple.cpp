#include "mim/plug/tuple/tuple.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

using namespace mim;

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axm%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"tuple", [](Normalizers& n) { plug::tuple::register_normalizers(n); }, nullptr, nullptr};
}
