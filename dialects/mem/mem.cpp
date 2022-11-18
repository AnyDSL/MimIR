#include "dialects/mem/mem.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

#include "dialects/mem/passes/fp/copy_prop.h"
#include "dialects/mem/passes/fp/ssa_constr.h"
#include "dialects/mem/passes/rw/alloc2malloc.h"
#include "dialects/mem/passes/rw/remem_elim.h"
#include "dialects/mem/passes/rw/reshape.h"
#include "dialects/mem/phases/rw/add_mem.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"mem",
            [](PipelineBuilder& builder) {
                builder.extend_opt_phase([](PassMan& man) {
                    auto br = man.add<BetaRed>();
                    auto er = man.add<EtaRed>();
                    auto ee = man.add<EtaExp>(er);
                    man.add<mem::SSAConstr>(ee);
                    man.add<mem::CopyProp>(br, ee);
                });
                builder.extend_codegen_prep_phase([](PassMan& man) {
                    // man.add<mem::AddMemWrapper>();
                    man.add<mem::RememElim>();
                    man.add<mem::Alloc2Malloc>();
                });
                // builder.extend_opt_phase(104, [](PassMan& man) { man.add<mem::Reshape>(mem::Reshape::Arg); });
                // builder.append_phase(130, [](Pipeline& pipeline) { pipeline.add<mem::AddMem>(); });

                // after AD, before closure conv
                // builder.extend_opt_phase(139, [](PassMan& man) { man.add<mem::Reshape>(mem::Reshape::Flat); });
            },
            nullptr, [](Normalizers& normalizers) { mem::register_normalizers(normalizers); }};
}
