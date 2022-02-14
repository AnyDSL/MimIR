#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/dce.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/alloc2malloc.h"
#include "thorin/pass/fp/unbox_closures.h"
#include "thorin/pass/fp/closure_analysis.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/remem_elim.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/pass/rw/drop_bb_closures.h"
#include "thorin/pass/rw/eta_cont.h"

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
    // opt.add<Scalerize>(ee);
    //opt.add<DCE>(br, ee);
    // opt.add<CopyProp>(br, ee);
    opt.run();


    PassMan closure_conv_prepare(world);
    closure_conv_prepare.add<EtaExp>(nullptr);
    closure_conv_prepare.add<RetWrap>();
    closure_conv_prepare.add<EtaCont>();
    closure_conv_prepare.run();

    // ClosureConv(world, ClosureConv::SSI, true).run();
    // world.debug_stream();
    
    // PassMan closure_destruct(world);
    // closure_destruct.add<DropBBClosures>();
    // closure_destruct.add<Scalerize>(nullptr);
    // closure_destruct.add<UnboxClosure>();
    // closure_destruct.run();

    // LowerTypedClosures(world).run();

    // PassMan codgen_prepare(world);
    //codgen_prepare.add<BoundElim>();
    // codgen_prepare.add<RememElim>();
    // codgen_prepare.add<Alloc2Malloc>();
    // codgen_prepare.add<RetWrap>();
    // codgen_prepare.run();
}

}
