#include "mim/plug/affine/affine.h"

#include <mim/config.h>
#include <mim/pass.h>

#include "mim/plug/affine/pass/lower_for.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Flags2Stages& stages) { Stage::hook<affine::lower_for_pass, affine::LowerFor>(stages); }

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"affine", nullptr, reg_stages, nullptr}; }
