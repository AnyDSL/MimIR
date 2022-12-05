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
                builder.extend_opt_phase(115, [](thorin::PassMan& man) { man.add<direct::DS2CPS>(); });
                builder.extend_opt_phase(116, [](thorin::PassMan& man) { man.add<direct::CPS2DS>(); });
                builder.add_opt(120);
            },
            [](Passes& passes) {
                register_pass<direct::ds2cps_pass, direct::DS2CPS>(passes);
                register_pass<direct::cps2ds_pass, direct::CPS2DS>(passes);
            },
            nullptr, [](Normalizers& normalizers) { direct::register_normalizers(normalizers); }};
}
