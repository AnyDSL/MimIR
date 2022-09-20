#include "thorin/pass/optimize.h"

// #include "thorin/pass/pass.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/phase/phase.h"

// #include "thorin/pass/fp/beta_red.h"
// #include "thorin/pass/fp/eta_exp.h"
// #include "thorin/pass/fp/eta_red.h"
// #include "thorin/pass/fp/tail_rec_elim.h"
// #include "thorin/pass/pass.h"
// #include "thorin/pass/rw/lam_spec.h"
// #include "thorin/pass/rw/partial_eval.h"
// #include "thorin/pass/rw/ret_wrap.h"
// #include "thorin/pass/rw/scalarize.h"

// #include "dialects/mem/passes/fp/copy_prop.h"
// #include "dialects/mem/passes/fp/ssa_constr.h"
// #include "dialects/mem/passes/rw/alloc2malloc.h"
// #include "dialects/mem/passes/rw/remem_elim.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/pass/rw/lam_spec.h"
// #include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

namespace thorin {

void optimize(World& world, PipelineBuilder& builder) {
    builder.extend_opt_phase(0, [](thorin::PassMan& man) { man.add<Scalerize>(); });
    builder.extend_opt_phase(1, [](thorin::PassMan& man) { man.add<EtaRed>(); });
    builder.extend_opt_phase(2, [](thorin::PassMan& man) { man.add<TailRecElim>(); });

    // main phase
    builder.add_opt(100);
    builder.extend_opt_phase(200, [](thorin::PassMan& man) { man.add<LamSpec>(); });
    // codegen prep phase
    builder.extend_opt_phase(
        300, [](thorin::PassMan& man) { man.add<RetWrap>(); }, 50);

    // builder.add_opt(110);
    // builder.add_opt(120);

    Pipeline pipe(world);

    auto passes = builder.passes();
    for (auto p : passes) {
        world.DLOG("Pass {}", p);
        pipe.add<PassManPhase>(builder.opt_phase(p, world));
    }

    // for (auto& p : pipe.phases()) { world.DLOG("Phase {}", p->name()); }

    // pipe.add<PassManPhase>(builder.opt_prep_phase1(world));
    // pipe.add<Scalerize>();
    // pipe.add<EtaRed>();
    // pipe.add<TailRecElim>();

    // {
    //     auto man = std::make_unique<PassMan>(world);
    //     // builder.add_opt(*man);
    //     man->add<PartialEval>();
    //     man->add<BetaRed>();
    //     auto er = man->add<EtaRed>();
    //     auto ee = man->add<EtaExp>(er);
    //     man->add<Scalerize>(ee);
    //     man->add<TailRecElim>(er);
    //     pipe.add<PassManPhase>(std::move(man));
    // }
    // // pipe.add<autodiff::AutoDiffEval>();
    // {
    //     auto man = std::make_unique<PassMan>(world);
    //     // builder.add_opt(*man);
    //     man->add<PartialEval>();
    //     man->add<BetaRed>();
    //     auto er = man->add<EtaRed>();
    //     auto ee = man->add<EtaExp>(er);
    //     man->add<Scalerize>(ee);
    //     man->add<TailRecElim>(er);
    //     pipe.add<PassManPhase>(std::move(man));
    // }
    // // pipe.add<autodiff::AutoDiffZero>();
    // // pipe.add<direct::DS2CPS>();
    // // pipe.add<direct::CPS2DS>();
    // {
    //     auto man = std::make_unique<PassMan>(world);
    //     // builder.add_opt(*man);
    //     man->add<PartialEval>();
    //     man->add<BetaRed>();
    //     auto er = man->add<EtaRed>();
    //     auto ee = man->add<EtaExp>(er);
    //     man->add<Scalerize>(ee);
    //     man->add<TailRecElim>(er);
    //     pipe.add<PassManPhase>(std::move(man));
    // }

    // pipe.add<PassManPhase>(builder.opt_prep_phase2(world));
    // pipe.add<PassManPhase>(builder.opt_phase(world));

    // pipe.add<LamSpec>();

    // {
    //     auto man = std::make_unique<PassMan>(world);
    //     man->add<RetWrap>();
    //     // zero cleanup
    //     // external cleanup
    //     pipe.add<PassManPhase>(std::move(man));
    // }

    // pipe.add<PassManPhase>(builder.codegen_prep_phase(world));
    // pipe.add<PassManPhase>(builder.opt_phase2(world));
    pipe.run();
}

} // namespace thorin
