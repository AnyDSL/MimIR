#include "thorin/pass/optimize.h"

#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/scalarize.h"

#if 0
#    include "dialects/clos/clos_conv.h"
#    include "dialects/clos/lower_typed_clos.h"
#    include "dialects/clos/pass/fp/lower_typed_clos_prep.h"
#    include "dialects/clos/pass/rw/branch_clos_elim.h"
#    include "dialects/clos/pass/rw/clos2sjlj.h"
#    include "dialects/clos/pass/rw/clos_conv_prep.h"
#endif

namespace thorin {

#if 0
static void closure_conv(World& world) {
    PassMan::run<ClosConvPrep>(world, nullptr);
    PassMan::run<EtaExp>(world, nullptr);

    ClosConv(world).run();

    PassMan cleanup(world);
    auto er = cleanup.add<EtaRed>(true); // We only want to eta-reduce things in callee position away at this point!
    auto ee = cleanup.add<EtaExp>(er);
    cleanup.add<Scalerize>(ee);
    cleanup.run();
}

static void lower_closures(World& world) {
    PassMan closure_destruct(world);
    closure_destruct.add<Scalerize>(nullptr);
    closure_destruct.add<BranchClosElim>();
    closure_destruct.add<CopyProp>(nullptr, nullptr, true);
    closure_destruct.add<LowerTypedClosPrep>();
    closure_destruct.add<Clos2SJLJ>();
    closure_destruct.run();

    LowerTypedClos(world).run();
}

#endif
void optimize(World& world, PipelineBuilder& builder) {}
#if 0
    // PassMan::run<Scalerize>(world, nullptr);
    // PassMan::run<EtaRed>(world);
    // PassMan::run<TailRecElim>(world, nullptr);

    PassMan opt(world);
    // opt.add<PartialEval>();
    // auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<SSAConstr>(ee);
    opt.add<Scalerize>(ee);
    // opt.add<CopyProp>(br, ee);
    // opt.add<TailRecElim>(er);
    opt.run();

    PassMan::run<Scalerize>(world, nullptr);
    PassMan::run<EtaRed>(world);
    PassMan::run<TailRecElim>(world, nullptr);

    auto opt = builder.opt_phase(world);
    opt->run();

    closure_conv(world);
    lower_closures(world);

    PassMan codgen_prepare(world);
    // codgen_prepare.add<BoundElim>();
    codgen_prepare.add<RememElim>();
    codgen_prepare.add<Alloc2Malloc>();
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();

    auto codegen_prep = builder.codegen_prep_phase(world);
    codegen_prep->run();
}
#endif

} // namespace thorin
