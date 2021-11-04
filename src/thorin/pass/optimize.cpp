#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/auto_diff.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

// old stuff
#include "thorin/transform/cleanup_world.h"
#include "thorin/transform/flatten_tuples.h"
#include "thorin/transform/partial_evaluation.h"
#include "thorin/transform/closure_conv.h"


//#define closure

namespace thorin {

void optimize(World& world) {
    world.set(LogLevel::Debug);

#ifdef closure
    PassMan opt(world);
    // opt.add<PartialEval>();
    // opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    // opt.add<SSAConstr>(ee);
    // opt.add<CopyProp>();
    // opt.add<Scalerize>();
    // opt.add<AutoDiff>();
    opt.run();

    ClosureConv(world).run();
    auto cc = PassMan(world);
    cc.add<Scalerize>();
    cc.run();
    world.debug_stream();

    // while (partial_evaluation(world, true)); // lower2cff
    // flatten_tuples(world);

    // PassMan codgen_prepare(world);
    //codgen_prepare.add<BoundElim>();
    // codgen_prepare.add<RetWrap>();
    // codgen_prepare.run();
#else

    PassMan opt(world);
    // opt.add<PartialEval>();
    // opt.add<BetaRed>();
    //    auto er = opt.add<EtaRed>();
    //    auto ee = opt.add<EtaExp>(er);
    // opt.add<SSAConstr>(ee);
    // opt.add<CopyProp>();
    // opt.add<Scalerize>();
    opt.add<AutoDiff>();
    opt.run();
    printf("Finished Opti1\n");

    //    ClosureConv(world).run();
    //    printf("Finished Closure\n");



    PassMan opt2(world);
    opt2.add<PartialEval>();
    opt2.add<BetaRed>();
    auto er = opt2.add<EtaRed>();
    auto ee = opt2.add<EtaExp>(er);
    opt2.add<SSAConstr>(ee);
//    opt2.add<Scalerize>(ee);
//    opt2.add<CopyProp>(ee);
    opt2.run();
    //    world.debug_stream();


    cleanup_world(world);
    while (partial_evaluation(world, true)); // lower2cff
    cleanup_world(world);

    PassMan codgen_prepare(world);
    //codgen_prepare.add<BoundElim>();
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();

#endif
}

}
