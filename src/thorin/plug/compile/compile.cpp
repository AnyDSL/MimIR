#include "thorin/plug/compile/compile.h"

#include <thorin/config.h>

#include <thorin/pass/beta_red.h>
#include <thorin/pass/eta_exp.h>
#include <thorin/pass/eta_red.h>
#include <thorin/pass/lam_spec.h>
#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>
#include <thorin/pass/ret_wrap.h>
#include <thorin/pass/scalarize.h>
#include <thorin/pass/tail_rec_elim.h>

#include "thorin/plug/compile/pass/debug_print.h"
#include "thorin/plug/compile/pass/internal_cleanup.h"

using namespace thorin;
using namespace thorin::plug;

void add_phases(Defs phases, World& world, Passes& passes, PipelineBuilder& builder) {
    for (auto phase : phases) compile::handle_optimization_part(phase, world, passes, builder);
}

void add_passes(World& world, PipelineBuilder& builder, Passes& passes, DefVec& pass_list) {
    // Concept: We create a convention that passes register in the pipeline using append_**in**_last.
    // This pass then calls the registered passes in the order they were registered in the last phase.

    // We create a new dummy phase in which the passes should be inserted.
    // builder.append_phase_end([](Pipeline&) {});
    builder.begin_pass_phase();
    for (auto pass : pass_list) compile::handle_optimization_part(pass, world, passes, builder);
    builder.end_pass_phase();
}

extern "C" THORIN_EXPORT thorin::Plugin thorin_get_plugin() {
    return {"compile", [](Normalizers& normalizers) { compile::register_normalizers(normalizers); },
            [](Passes& passes) {
                auto debug_phase_flag = flags_t(Annex::Base<thorin::plug::compile::debug_phase>);
                assert_emplace(passes, debug_phase_flag, [](World& world, PipelineBuilder& builder, const Def* app) {
                    world.DLOG("Generate debug_phase: {}", app);
                    int level = (int)(app->as<App>()->arg(0)->as<Lit>()->get<u64>());
                    world.DLOG("  Level: {}", level);
                    builder.add_phase<compile::DebugPrint>(level);
                });

                assert_emplace(passes, flags_t(Annex::Base<thorin::plug::compile::passes_to_phase>),
                               [&](World& world, PipelineBuilder& builder, const Def* app) {
                                   auto pass_array = app->as<App>()->arg()->projs();
                                   DefVec pass_list;
                                   for (auto pass : pass_array) pass_list.push_back(pass);
                                   add_passes(world, builder, passes, pass_list);
                               });

                assert_emplace(passes, flags_t(Annex::Base<thorin::plug::compile::phases_to_phase>),
                               [&](World& world, PipelineBuilder& builder, const Def* app) {
                                   auto phase_array = app->as<App>()->arg()->projs();
                                   DefVec phase_list;
                                   for (auto phase : phase_array) phase_list.push_back(phase);
                                   add_phases(phase_list, world, passes, builder);
                               });

                assert_emplace(passes, flags_t(Annex::Base<thorin::plug::compile::pipe>),
                               [&](World& world, PipelineBuilder& builder, const Def* app) {
                                   auto [ax, phases] = collect_args(app);
                                   add_phases(phases, world, passes, builder);
                               });
                assert_emplace(
                    passes, flags_t(Annex::Base<thorin::plug::compile::nullptr_pass>),
                    [](World&, PipelineBuilder& builder, const Def* def) { builder.def2pass(def, nullptr); });

                register_pass<compile::beta_red_pass, BetaRed>(passes);
                register_pass<compile::eta_red_pass, EtaRed>(passes);

                register_pass<compile::lam_spec_pass, LamSpec>(passes);
                register_pass<compile::ret_wrap_pass, RetWrap>(passes);
                register_pass<compile::internal_cleanup_pass, compile::InternalCleanup>(passes);

                register_pass_with_arg<compile::eta_exp_pass, EtaExp, EtaRed>(passes);
                register_pass_with_arg<compile::scalerize_pass, Scalarize, EtaExp>(passes);
                register_pass_with_arg<compile::tail_rec_elim_pass, TailRecElim, EtaRed>(passes);
            },
            nullptr};
}
