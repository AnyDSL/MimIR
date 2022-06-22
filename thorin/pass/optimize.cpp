#include "thorin/pass/optimize.h"

#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/pass/rw/auto_diff.h"

#include "thorin/error.h"

namespace thorin {

void optimize(World& world, PipelineBuilder& builder) {
    PassMan::run<Scalerize>(world, nullptr);
    PassMan::run<EtaRed>(world);
    PassMan::run<TailRecElim>(world, nullptr);

    PassMan::run<AutoDiff>(world);

    auto opt = builder.opt_phase(world);
    opt->run();

    PassMan::run<LamSpec>(world);

    auto codegen_prep = builder.codegen_prep_phase(world);
    codegen_prep->run();
}


} // namespace thorin
