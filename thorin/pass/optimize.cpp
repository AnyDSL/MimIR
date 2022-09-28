#include "thorin/pass/optimize.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/phase/phase.h"

namespace thorin {

/// The optimizations proceed in a pipeline ordered by priorities.
/// Each phase is a sequence of passes that are run interleaved.
/// The passes are added to the phase ordered by their priority.

/// Order:
/// * 1-10: initial passes
/// * 100: Main optimization phase (default for extend_opt_phase)
/// * 200: Pre-CodeGen Optimization
/// * 300: CodeGen Preparation (default for extend_codegen_prep_phase)
///
/// concrete phases:
/// * 0  : Scalarize
/// * 1  : EtaRed
/// * 2  : TailRecElim
/// * 100: Optimize (Priority 50)
///   * PartialEval
///   * BetaRed
///   * EtaRed
///   * EtaExp
///   * Scalarize
///   * TailRecElim
///   * + Custom (default priority 100)
/// * 200: LamSpec
/// * 300: RetWrap (Priority 50)
///   * + Custom (default priority 100)

/// See optimize.h for magic numbers
void optimize(World& world, PipelineBuilder& builder) {
    builder.extend_opt_phase(0, [](thorin::PassMan& man) { man.add<Scalerize>(); });
    builder.extend_opt_phase(1, [](thorin::PassMan& man) { man.add<EtaRed>(); });
    builder.extend_opt_phase(2, [](thorin::PassMan& man) { man.add<TailRecElim>(); });

    // main phase
    builder.add_opt(Opt_Phase);
    builder.extend_opt_phase(200, [](thorin::PassMan& man) { man.add<LamSpec>(); });
    // codegen prep phase
    builder.extend_opt_phase(
        Codegen_Prep_Phase, [](thorin::PassMan& man) { man.add<RetWrap>(); }, Pass_Internal_Priority);

    Pipeline pipe(world);

    auto passes = builder.passes();
    for (auto p : passes) pipe.add<PassManPhase>(builder.opt_phase(p, world));

    pipe.run();
}

} // namespace thorin
