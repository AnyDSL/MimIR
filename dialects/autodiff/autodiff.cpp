#include "dialects/autodiff/autodiff.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/autodiff/passes/autodiff_zero.h"
#include "dialects/autodiff/passes/autodiff_zero_cleanup.h"
#include "dialects/direct/passes/ds2cps.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"autodiff",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase([](thorin::PassMan& man) {
                    man.add<thorin::autodiff::AutoDiffEval>();
                    // in theory only after partial eval (beta, ...)
                    // but before other simplification
                    // zero and add need to be close together
                    man.add<thorin::autodiff::AutoDiffZero>();
                });
                builder.extend_codegen_prep_phase(
                    [](PassMan& man) { man.add<thorin::autodiff::AutoDiffZeroCleanup>(); });
            },
            nullptr, [](Normalizers& normalizers) { autodiff::register_normalizers(normalizers); }};
}
