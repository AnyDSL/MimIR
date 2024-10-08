#include "mim/plug/math/math.h"

#include <mim/config.h>
#include <mim/pass/pass.h>

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"math", [](Normalizers& normalizers) { plug::math::register_normalizers(normalizers); }, nullptr, nullptr};
}
