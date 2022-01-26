#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/dce.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

#include "thorin/pass/fp/closure_destruct.h"
#include "thorin/pass/fp/unbox_closures.h"
#include "thorin/pass/fp/closure_analysis.h"

// old stuff
#include "thorin/transform/cleanup_world.h"
#include "thorin/transform/partial_evaluation.h"
#include "thorin/transform/closure_conv.h"
#include "thorin/transform/lower_typed_closures.h"

namespace thorin {

void optimize(World& world) {
    PassMan opt(world);
    // opt.add<PartialEval>();
    // opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<SSAConstr>(ee);
    // opt.add<CopyProp>();
    opt.add<Scalerize>(ee);
    // opt.add<AutoDiff>();
    opt.run();

    PassMan closure_ana(world);
    closure_ana.add<ClosureAnalysis>();
    closure_ana.run();

    // while (partial_evaluation(world, true)); // lower2cff
    // world.debug_stream();
    
    ClosureConv(world).run();
    world.debug_stream();
    
    // PassMan closure_destruct(world);
    // er = closure_destruct.add<EtaRed>();
    // ee = closure_destruct.add<EtaExp>(er);
    // closure_destruct.add<ClosureDestruct>(ee);
    // closure_destruct.add<Scalerize>(nullptr);
    // closure_destruct.run();

    // PassMan unbox_closures(world);
    // unbox_closures.add<Scalerize>(nullptr);
    // unbox_closures.add<UnboxClosure>();
    // unbox_closures.run();

    // LowerTypedClosures(world).run();

    // PassMan codgen_prepare(world);
    // codgen_prepare.add<Scalerize>(nullptr);
    // codgen_prepare.add<RetWrap>();
    // codgen_prepare.run();
    world.debug_stream();
}
}
