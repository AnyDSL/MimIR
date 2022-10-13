#include "dialects/autodiff/autodiff.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/autodiff/passes/autodiff_ext_cleanup.h"
#include "dialects/autodiff/passes/autodiff_zero.h"
#include "dialects/autodiff/passes/autodiff_zero_cleanup.h"
#include "dialects/direct/passes/ds2cps.h"

using namespace thorin;

/// optimization idea:
/// * optimize [100]
/// * perform ad [105]
/// * resolve unsolved zeros (not added) [111]
/// * optimize further, cleanup direct style [115-120] (in direct)
/// * cleanup (zeros, externals) [299]
extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"autodiff",
            [](thorin::PipelineBuilder& builder) {
                builder.add_opt(110);
                builder.extend_opt_phase(105, [](thorin::PassMan& man) { man.add<thorin::autodiff::AutoDiffEval>(); });
                builder.extend_opt_phase(111, [](thorin::PassMan& man) {
                    // in theory only after partial eval (beta, ...)
                    // but before other simplification
                    // zero and add need to be close together
                    man.add<thorin::autodiff::AutoDiffZero>();
                });
                builder.extend_opt_phase(299, [](PassMan& man) {
                    man.add<thorin::autodiff::AutoDiffZeroCleanup>();
                    man.add<thorin::autodiff::AutoDiffExternalCleanup>();
                });
            },
            nullptr, [](Normalizers& normalizers) { autodiff::register_normalizers(normalizers); }};
}
