#include "mim/plug/demo/demo.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

using namespace mim;

/// Registers normalizers as well as Phase%s and Pass%es for the Axm%s of this Plugin.
extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"demo", plug::demo::register_normalizers, nullptr, nullptr}; }
