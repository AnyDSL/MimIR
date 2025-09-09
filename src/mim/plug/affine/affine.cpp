#include "mim/plug/affine/affine.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"affine", nullptr, nullptr}; }
