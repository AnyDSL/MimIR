#include "thorin/pass/optimize.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/alloc2malloc.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/remem_elim.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

namespace thorin {

void optimize(World& world) {
    PassMan opt(world);
    opt.add<PartialEval>();
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<SSAConstr>(ee);
    opt.add<Scalerize>(ee);
    opt.add<CopyProp>(br, ee);
    opt.add<TailRecElim>(er);
    opt.add<LamSpec>();
    opt.run();

    PassMan codgen_prepare(world);
    // codgen_prepare.add<BoundElim>();
    codgen_prepare.add<RememElim>();
    codgen_prepare.add<Alloc2Malloc>();
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();
}

} // namespace thorin
