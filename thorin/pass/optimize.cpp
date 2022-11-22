#include "thorin/pass/optimize.h"

#include "thorin/dialects.h"

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

// TODO: move
/// Helper function to cope with the fact that normalizers take all arguments and not only its axiom arguments.
std::pair<const Def*, std::vector<const Def*>> collect_args(const Def* def) {
    std::vector<const Def*> args;
    if (auto app = def->isa<App>()) {
        auto callee               = app->callee();
        auto arg                  = app->arg();
        auto [inner_callee, args] = collect_args(callee);
        args.push_back(arg);
        return {inner_callee, args};
    } else {
        return {def, args};
    }
}

void addPhases(DefVec& phases, World& world, Passes& passes, PipelineBuilder& builder) {
    for (auto phase : phases) {
        // world.DLOG("phase: {}", phase);
        auto [phase_def, phase_args] = collect_args(phase);
        world.DLOG("phase: {}", phase_def);
        // world.DLOG("  args: {,}", phase_args);
        if (auto phase_ax = phase_def->isa<Axiom>()) {
            auto flag = phase_ax->flags();
            // world.DLOG("axiom flag: {}", flag);
            if (passes.contains(flag)) {
                // world.DLOG("found registered phase");
                auto phase_fun = passes[flag];
                phase_fun(world, builder, phase);
            }else{
                world.WLOG("phase '{}' not found", phase_ax->name());
            }
        }else {
            world.WLOG("phase '{}' is not an axiom", phase_def);
        }
    }
}

/// See optimize.h for magic numbers
void optimize(World& world, Passes& passes, PipelineBuilder& builder) {
    if (auto compilation = world.lookup("_compile")) {
        world.DLOG("compilation using {} : {}", compilation, compilation->type());
        compilation->make_internal();

        // We can not directly access compile axioms here.
        // But the compile dialect has not the necessary communication pipeline.
        // One idea would be that the register pass phase in the compile dialect builds the pipeline.
        // But we do not have access to the necessary information there.

        PipelineBuilder pipe_builder;

        // TODO: generalize this hardcoded special case
        auto pipeline     = compilation->as<Lam>()->body();
        auto [ax, phases] = collect_args(pipeline);
        // TODO: handle passes not only phases
        addPhases(phases, world, passes, pipe_builder);

        Pipeline pipe(world);
        pipe_builder.buildPipeline(pipe);

        pipe.run();
        return;
    }

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

    // auto passes = builder.passes();
    // for (auto p : passes) pipe.add<PassManPhase>(builder.opt_phase(p, world));
    builder.buildPipeline(pipe);

    pipe.run();
}

} // namespace thorin
