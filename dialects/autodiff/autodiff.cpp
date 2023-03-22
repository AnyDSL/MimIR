#include "dialects/autodiff/autodiff.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>

#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/autodiff/passes/autodiff_ext_cleanup.h"
#include "dialects/autodiff/passes/autodiff_zero.h"
#include "dialects/autodiff/passes/autodiff_zero_cleanup.h"
#include "dialects/direct/passes/ds2cps.h"

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"autodiff",
            [](Passes& passes) {
                register_pass<autodiff::ad_eval_pass, autodiff::AutoDiffEval>(passes);
                register_pass<autodiff::ad_zero_pass, autodiff::AutoDiffZero>(passes);
                register_pass<autodiff::ad_zero_cleanup_pass, autodiff::AutoDiffZeroCleanup>(passes);
                register_pass<autodiff::ad_ext_cleanup_pass, autodiff::AutoDiffExternalCleanup>(passes);
            },
            nullptr, [](Normalizers& normalizers) { autodiff::register_normalizers(normalizers); }};
}
