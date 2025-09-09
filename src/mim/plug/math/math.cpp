#include "mim/plug/math/math.h"

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"math", plug::math::register_normalizers, nullptr}; }
