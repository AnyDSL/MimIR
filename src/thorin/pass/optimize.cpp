#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/pass/rw/auto_diff.h"

// old stuff
#include "thorin/transform/cleanup_world.h"
#include "thorin/transform/partial_evaluation.h"
#include "thorin/transform/closure_conv.h"

#include "thorin/transform/closure_conv.h"

namespace thorin {

void optimize(World& world) {
    world.set(LogLevel::Debug);

//     std::ofstream ofile("output.txt");
//     std::shared_ptr<Stream> s(new Stream(ofile));
//    world.set(s);

    PassMan opt(world);
//    opt.add<PartialEval>();
//    opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<SSAConstr>(ee);
    opt.add<CopyProp>(ee);

    printf("Start Opti1\n");
//    opt.run();
    // do not run opt yet as it destroys complicated behaviour interesting for autodiff
    //  otherwise the behavior is not completely eliminated but only hidden in obscure contexts
    printf("Finished Opti1\n");

//            ClosureConv cc(world);
//            cc.run();


    PassMan opt3(world);
    opt3.add<AutoDiff>();
    opt3.run();

    PassMan opt2(world);
    opt2.add<PartialEval>();
    opt2.add<BetaRed>();
    auto er2 = opt2.add<EtaRed>();
    auto ee2 = opt2.add<EtaExp>(er2);
    opt2.add<SSAConstr>(ee2);
    //    opt2.add<Scalerize>(ee2);

//    opt2.add<AutoDiff>();

    printf("Start Opti2\n");
    opt2.run();
    printf("Finished Opti2\n");





    cleanup_world(world);
    while (partial_evaluation(world, true)); // lower2cff
    cleanup_world(world);
    printf("Finished Opti2\n");

    PassMan codgen_prepare(world);
    //codgen_prepare.add<BoundElim>();
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();

//            ClosureConv cc(world);
//            cc.run();
}

}
