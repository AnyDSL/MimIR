#include "mim/plug/tuple/tuple.h"

#include <mim/plugin.h>

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"tuple", plug::tuple::register_normalizers, nullptr}; }
