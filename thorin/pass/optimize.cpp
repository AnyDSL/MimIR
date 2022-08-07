#include "thorin/pass/optimize.h"

#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/scalarize.h"

#include "thorin/phase/phase.h"

namespace thorin {

void optimize(World& world, PipelineBuilder& builder) {
    Pipeline pipe(world);
    pipe.add<Scalerize>();
    pipe.add<EtaRed>();
    pipe.add<TailRecElim>();
    pipe.run();

    auto opt = builder.opt_phase(world);
    opt->run();

    PassMan::run<LamSpec>(world);

    auto codegen_prep = builder.codegen_prep_phase(world);
    codegen_prep->run();
}

} // namespace thorin
