#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/dce.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/auto_diff.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/alloc2malloc.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/remem_elim.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/pass/fp/zip_eval.h"

// old stuff
#include "thorin/transform/cleanup_world.h"
#include "thorin/transform/partial_evaluation.h"
#include "thorin/transform/mangle.h"


#include "thorin/error.h"

namespace thorin {

void optimize(World& world) {

    world.set(LogLevel::Debug);
    // world.set(std::make_unique<ErrorHandler>());
//    std::unique_ptr<ErrorHandler> err;
//    ErrorHandler* err;
    world.set((std::unique_ptr<ErrorHandler>&&) nullptr);

    PassMan opt(world);
    opt.add<AutoDiff>();

//     PassMan optZ(world);
//     optZ.add<ZipEval>();
//     optZ.run();
//     printf("Finished OptiZip\n");

//     return;


    PassMan opt2(world);
    auto br = opt2.add<BetaRed>();
    auto er = opt2.add<EtaRed>();
    auto ee = opt2.add<EtaExp>(er);
    opt2.add<SSAConstr>(ee);
    opt2.add<Scalerize>(ee);
    // opt2.add<DCE>(br, ee);
    opt2.add<CopyProp>(br, ee);
    opt2.add<TailRecElim>(er);
//    opt2.run();
    printf("Finished Prepare Opti\n");


    opt.run();
    printf("Finished AutoDiff Opti\n");

    PassMan opt3(world);
    auto br3 = opt3.add<BetaRed>();
    auto er3 = opt3.add<EtaRed>();
    auto ee3 = opt3.add<EtaExp>(er);
    opt3.add<SSAConstr>(ee3);
    opt3.add<Scalerize>(ee3);
    // opt3.add<DCE>(br3, ee3);
    opt3.add<CopyProp>(br3, ee3);
    opt3.add<TailRecElim>(er3);
    opt3.run();
    printf("Finished Simpl Opti\n");

        cleanup_world(world);
//     partial_evaluation(world, true);
    while (partial_evaluation(world, true)) {} // lower2cff
        cleanup_world(world);

    printf("Finished Cleanup\n");

    PassMan codgen_prepare(world);
    // codgen_prepare.add<BoundElim>();
    codgen_prepare.add<RememElim>();
    codgen_prepare.add<Alloc2Malloc>();
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();
}

} // namespace thorin
