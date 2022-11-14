#include "dialects/autodiff/autodiff.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/affine/passes/lower_for.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/autodiff/passes/autodiff_reduce.h"
#include "dialects/autodiff/passes/autodiff_reduce_free.h"
#include "dialects/mem/passes/rw/reshape.h"

using namespace thorin;

class DebugWrapper : public RWPass<DebugWrapper, Lam> {
public:
    DebugWrapper(PassMan& man)
        : RWPass(man, "debug_pass") {}

    void prepare() override { world().debug_dump(); }
};

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"autodiff",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase(107,
                                         [](thorin::PassMan& man) { man.add<thorin::autodiff::AutodiffReduce>(); });
                builder.extend_opt_phase(108,
                                         [](thorin::PassMan& man) { man.add<thorin::autodiff::AutodiffReduceFree>(); });
                builder.extend_opt_phase(109, [](thorin::PassMan& man) { man.add<thorin::autodiff::AutoDiffEval>(); });
                builder.add_opt(133);
            },
            nullptr, [](Normalizers& normalizers) { autodiff::register_normalizers(normalizers); }};
}
