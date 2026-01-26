#include "mim/plug/gpu/gpu.h"

#include <mim/plugin.h>

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"gpu", nullptr, nullptr, nullptr}; }
