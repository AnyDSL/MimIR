#include "mim/plug/opt/opt.h"

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"opt", nullptr, nullptr, nullptr}; }
