#include "thorin/pass/optimize.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/alloc2malloc.h"
#include "thorin/pass/rw/branch_clos_elim.h"
#include "thorin/pass/fp/lower_typed_clos_prep.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/remem_elim.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/pass/rw/clos_conv_prep.h"
#include "thorin/pass/rw/clos2sjlj.h"

<<<<<<< HEAD
// old stuff
#include "thorin/transform/cleanup_world.h"
#include "thorin/transform/partial_evaluation.h"
#include "thorin/transform/clos_conv.h"
#include "thorin/transform/lower_typed_clos.h"

=======
>>>>>>> master
namespace thorin {

static void closure_conv(World& world) {
    PassMan prepare(world);
    auto ee = prepare.add<EtaExp>(nullptr);
    prepare.add<ClosConvPrep>(ee);
    prepare.run();

    ClosConv(world).run();

    PassMan cleanup(world);
    auto er = cleanup.add<EtaRed>(true); // We only want to eta-reduce things in callee position away at this point!
    ee = cleanup.add<EtaExp>(er);
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

void optimize(World& world) {
    PassMan::run<Scalerize>(world, nullptr);
    PassMan::run<EtaRed>(world);
    PassMan::run<TailRecElim>(world, nullptr);

    PassMan opt(world);
    // opt.add<PartialEval>();
    // auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    // opt.add<SSAConstr>(ee);
    // opt.add<Scalerize>(ee);
    // opt.add<CopyProp>(br, ee);
    // opt.add<TailRecElim>(er);
    opt.run();

<<<<<<< HEAD
    closure_conv(world);
    lower_closures(world);
    
    PassMan codgen_prepare(world);
    // codgen_prepare.add<BoundElim>();
    codgen_prepare.add<RememElim>();
    codgen_prepare.add<Alloc2Malloc>();
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();
=======
    PassMan::run<LamSpec>(world);

    PassMan codgen_prep(world);
    // codgen_prep.add<BoundElim>();
    codgen_prep.add<RememElim>();
    codgen_prep.add<Alloc2Malloc>();
    codgen_prep.add<RetWrap>();
    codgen_prep.run();
>>>>>>> master
}

} // namespace thorin
