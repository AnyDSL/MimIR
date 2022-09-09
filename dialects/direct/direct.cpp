#include "dialects/direct/direct.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

#include "dialects/direct/passes/cps2ds.h"
#include "dialects/direct/passes/ds2cps.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"direct",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase([](thorin::PassMan& man) {
                    man.add<direct::DS2CPS>();
                    // auto ds2cps = man.add<direct::DS2CPS>();
                    // man.add<direct::DSCall>(ds2cps);
                });
                builder.extend_opt_phase([](thorin::PassMan& man) { man.add<direct::CPS2DS>(); });
                builder.extend_codegen_prep_phase([](thorin::PassMan& man) {
                    man.add<direct::CPS2DS>();
                    man.add<PartialEval>();
                    man.add<BetaRed>();
                    auto er = man.add<EtaRed>();
                    auto ee = man.add<EtaExp>(er);
                    man.add<Scalerize>(ee);
                    man.add<TailRecElim>(er);
                });
            },
            nullptr, [](Normalizers& normalizers) { direct::register_normalizers(normalizers); }};
}

namespace thorin::direct {} // namespace thorin::direct