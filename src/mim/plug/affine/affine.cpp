#include "mim/plug/affine/affine.h"

#include <mim/config.h>

#include <mim/pass/pass.h>
#include <mim/pass/pipelinebuilder.h>

#include "mim/plug/affine/pass/lower_for.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"affine", nullptr, [](Passes& passes) { register_pass<affine::lower_for_pass, affine::LowerFor>(passes); },
            nullptr};
}
