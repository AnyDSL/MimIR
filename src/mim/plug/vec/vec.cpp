#include "mim/plug/vec/vec.h"

#include <mim/plugin.h>

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"vec", plug::vec::register_normalizers, nullptr}; }
