#include "dialects/compile/compile.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

#include "dialects/compile/autogen.h"
#include "dialects/compile/passes/debug_print.h"

using namespace thorin;

void add_phases(DefVec& phases, World& world, Passes& passes, PipelineBuilder& builder) {
    for (auto phase : phases) {
        auto [phase_def, phase_args] = collect_args(phase);
        world.DLOG("phase: {}", phase_def);
        if (auto phase_ax = phase_def->isa<Axiom>()) {
            auto flag = phase_ax->flags();
            if (passes.contains(flag)) {
                auto phase_fun = passes[flag];
                phase_fun(world, builder, phase);
            } else {
                world.WLOG("phase '{}' not found", phase_ax->name());
            }
        } else {
            world.WLOG("phase '{}' is not an axiom", phase_def);
        }
    }
}

void add_passes(World& world, PipelineBuilder& builder, Passes& passes, DefVec& pass_list) {
    // Concept: We create a convention that passes register in the pipeline using append_**in**_last.
    // This pass then calls the registered passes in the order they were registered in the last phase.

    // We create a new dummy phase in which the passes should be inserted.
    builder.append_phase_end([](Pipeline&) {});

    for (auto pass : pass_list) {
        auto [pass_def, pass_args] = collect_args(pass);
        world.DLOG("pass: {}", pass_def);
        if (auto pass_ax = pass_def->isa<Axiom>()) {
            auto flag = pass_ax->flags();
            if (passes.contains(flag)) {
                auto pass_fun = passes[flag];
                pass_fun(world, builder, pass);
            } else {
                world.ELOG("pass '{}' not found", pass_ax->name());
            }
        } else {
            world.ELOG("pass '{}' is not an axiom", pass_def);
        }
    }
}

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"compile", nullptr,
            [](Passes& passes) {
                auto debug_phase_flag    = flags_t(Axiom::Base<thorin::compile::debug_phase>);
                passes[debug_phase_flag] = [](World& world, PipelineBuilder& builder, const Def* app) {
                    world.DLOG("Generate debug_phase: {}", app);
                    int level = (int)(app->as<App>()->arg(0)->as<Lit>()->get<u64>());
                    world.DLOG("  Level: {}", level);
                    builder.append_pass_after_end([=](PassMan& man) { man.add<thorin::compile::DebugPrint>(level); });
                };

                passes[flags_t(Axiom::Base<thorin::compile::passes_to_phase>)] =
                    [&](World& world, PipelineBuilder& builder, const Def* app) {
                        auto pass_array = app->as<App>()->arg()->projs();
                        DefVec pass_list;
                        for (auto pass : pass_array) { pass_list.push_back(pass); }
                        add_passes(world, builder, passes, pass_list);
                    };

                passes[flags_t(Axiom::Base<thorin::compile::phases_to_phase>)] =
                    [&](World& world, PipelineBuilder& builder, const Def* app) {
                        auto phase_array = app->as<App>()->arg()->projs();
                        DefVec phase_list;
                        for (auto phase : phase_array) { phase_list.push_back(phase); }
                        add_phases(phase_list, world, passes, builder);
                    };

                passes[flags_t(Axiom::Base<thorin::compile::pipe>)] = [&](World& world, PipelineBuilder& builder,
                                                                          const Def* app) {
                    auto [ax, phases] = collect_args(app);
                    add_phases(phases, world, passes, builder);
                };
                passes[flags_t(Axiom::Base<thorin::compile::nullptr_pass>)] = [&](World&, PipelineBuilder& builder,
                                                                                  const Def* def) {
                    builder.remember_pass_instance(nullptr, def);
                };

                register_pass<compile::partial_eval_pass, PartialEval>(passes);
                register_pass<compile::beta_red_pass, BetaRed>(passes);
                register_pass<compile::eta_red_pass, EtaRed>(passes);

                register_pass<compile::lam_spec_pass, LamSpec>(passes);
                register_pass<compile::ret_wrap_pass, RetWrap>(passes);

                register_pass_with_arg<compile::eta_exp_pass, EtaExp, EtaRed>(passes);
                register_pass_with_arg<compile::scalerize_pass, Scalerize, EtaExp>(passes);
                register_pass_with_arg<compile::tail_rec_elim_pass, TailRecElim, EtaRed>(passes);
            },
            nullptr, [](Normalizers& normalizers) { compile::register_normalizers(normalizers); }};
}
