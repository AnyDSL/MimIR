#include "thorin/pass/optimize.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/dce.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/alloc2malloc.h"
#include "thorin/pass/fp/unbox_closures.h"
#include "thorin/pass/fp/closure_analysis.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/remem_elim.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/pass/rw/cconv_prepare.h"
#include "thorin/pass/rw/closure2sjlj.h"

// old stuff
#include "thorin/transform/cleanup_world.h"
#include "thorin/transform/partial_evaluation.h"
#include "thorin/transform/closure_conv.h"
#include "thorin/transform/lower_typed_closures.h"

namespace thorin {

static void closure_conv(World& world) {
    PassMan prepare(world);
    auto ee = prepare.add<EtaExp>(nullptr);
    prepare.add<CConvPrepare>(ee);
    prepare.run();

    ClosureConv(world).run();

    PassMan cleanup(world);
    auto er = cleanup.add<EtaRed>();
    ee = cleanup.add<EtaExp>(er);
    cleanup.add<Scalerize>(ee);
    cleanup.run();
}

static void lower_closures(World& world) {
    PassMan closure_destruct(world);
    closure_destruct.add<Scalerize>(nullptr);
    closure_destruct.add<UnboxClosure>();
    closure_destruct.add<CopyProp>(nullptr, nullptr, true);
    closure_destruct.add<ClosureAnalysis>();
    closure_destruct.add<Closure2SjLj>();
    closure_destruct.run();

    LowerTypedClosures(world).run();
}

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
    // opt.add<TailRecElim>(er);
    opt.run();

    closure_conv(world);
    lower_closures(world);
    
    PassMan codgen_prepare(world);
    // codgen_prepare.add<BoundElim>();
    codgen_prepare.add<RememElim>();
    codgen_prepare.add<Alloc2Malloc>();
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();
}

} // namespace thorin
