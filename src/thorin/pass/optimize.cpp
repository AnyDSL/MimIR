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


#define closure

namespace thorin {

void optimize(World& world) {
    world.set(LogLevel::Debug);

//     std::ofstream ofile("output.txt");
//     std::shared_ptr<Stream> s(new Stream(ofile));
//    world.set(s);


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
    printf("Finished Opti1\n");

    ClosureConv(world).run();

    printf("Finished Closure\n");

    auto cc = PassMan(world);
    auto er2 = opt.add<EtaRed>();
    auto ee2 = opt.add<EtaExp>(er2);
    cc.add<Scalerize>(ee2);
    cc.run();
    printf("Finished Opti2\n");
//    world.debug_stream();

    // while (partial_evaluation(world, true)); // lower2cff
    // flatten_tuples(world);

    // PassMan codgen_prepare(world);
    //codgen_prepare.add<BoundElim>();
    // codgen_prepare.add<RetWrap>();
    // codgen_prepare.run();

}

}
