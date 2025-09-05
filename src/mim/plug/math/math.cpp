#include "mim/plug/math/math.h"

#include <mim/config.h>
#include <mim/pass.h>

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"math", plug::math::register_normalizers, nullptr, nullptr}; }
