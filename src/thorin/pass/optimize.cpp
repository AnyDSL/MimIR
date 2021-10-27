#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/scalarize.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

// old stuff
#include "thorin/transform/cleanup_world.h"
#include "thorin/transform/partial_evaluation.h"

namespace thorin {

void optimize(World& world) {
    PassMan opt(world);
    opt.add<PartialEval>();
    opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<SSAConstr>(ee);

    opt.add<Scalerize>();
    //opt.add<CopyProp>();
//    opt.add<AutoDiff>();

    printf("Start Opti\n");
    opt.run();
    printf("Finished Opti1\n");

    cleanup_world(world);
    while (partial_evaluation(world, true)); // lower2cff
    cleanup_world(world);
    printf("Finished Opti2\n");

    PassMan codgen_prepare(world);
    //codgen_prepare.add<BoundElim>();
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();
}

}
