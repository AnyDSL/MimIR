#include "mim/plug/ord/ord.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"ord", plug::ord::register_normalizers, nullptr, nullptr}; }
