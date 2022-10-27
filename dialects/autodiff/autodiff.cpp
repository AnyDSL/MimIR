#include "dialects/autodiff/autodiff.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/affine/passes/lower_for.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/autodiff/passes/autodiff_ext_cleanup.h"
#include "dialects/direct/passes/ds2cps.h"
#include "dialects/mem/passes/rw/reshape.h"

using namespace thorin;

class DebugWrapper : public RWPass<DebugWrapper, Lam> {
public:
    DebugWrapper(PassMan& man)
        : RWPass(man, "debug_pass") {}

    void prepare() override { world().debug_dump(); }
};

/// optimization idea:
/// * optimize [100]
/// * perform ad [105]
/// * resolve unsolved zeros (not added) [111]
/// * optimize further, cleanup direct style [115-120] (in direct)
/// * cleanup (zeros, externals) [299]
extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"autodiff",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase(107, [](thorin::PassMan& man) { man.add<thorin::autodiff::AutoDiffEval>(); });
                // builder.extend_opt_phase(126, [](PassMan& man) {
                // man.add<thorin::autodiff::AutoDiffExternalCleanup>(); });

                builder.add_opt(126);
                builder.extend_opt_phase(
                    131, [](thorin::PassMan& man) { man.add<thorin::mem::Reshape>(thorin::mem::Reshape::Flat); });
                builder.add_opt(132);
                builder.extend_opt_phase(133, [](thorin::PassMan& man) { man.add<DebugWrapper>(); });
            },
            nullptr, [](Normalizers& normalizers) { autodiff::register_normalizers(normalizers); }};
}
