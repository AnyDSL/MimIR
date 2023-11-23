#include "thorin/plug/affine/affine.h"

#include <thorin/analyses/scope.h>
#include <thorin/config.h>
#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>
#include <thorin/rewrite.h>

#include "thorin/plug/affine/pass/lower_for.h"

using namespace thorin;
using namespace thorin::plug;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"affine", nullptr, [](Passes& passes) { register_pass<affine::lower_for_pass, affine::LowerFor>(passes); },
            nullptr};
}
