#include "dialects/compile/compile.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

#include "dialects/compile/passes/debug_print.h"

using namespace thorin;

// TODO: move
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

void addPasses(World& world, PipelineBuilder& builder, Passes& passes, DefVec& pass_list) {
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
                world.WLOG("pass '{}' not found", pass_ax->name());
            }
        } else {
            world.WLOG("pass '{}' is not an axiom", pass_def);
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
                    // TODO: add a debug pass to the pipeline
                    builder.append_pass_after_end([&](PassMan& man) {
                        man.add<thorin::compile::DebugPrint>(level);
                        // man.add<thorin::compile::DebugPrint>(1);
                    });
                };

                auto pass_phase_flag    = flags_t(Axiom::Base<thorin::compile::pass_phase>);
                passes[pass_phase_flag] = [&](World& world, PipelineBuilder& builder, const Def* app) {
                    auto [ax, pass_list] = collect_args(app->as<App>()->arg());
                    addPasses(world, builder, passes, pass_list);
                };

                auto combined_phase_flag    = flags_t(Axiom::Base<thorin::compile::combined_phase>);
                passes[combined_phase_flag] = [&](World& world, PipelineBuilder& builder, const Def* app) {
                    auto [ax, phase_list] = collect_args(app->as<App>()->arg());
                    addPhases(phase_list, world, passes, builder);
                };

                passes[flags_t(Axiom::Base<thorin::compile::passes_to_phase>)] =
                    [&](World& world, PipelineBuilder& builder, const Def* app) {
                        auto pass_array = app->as<App>()->arg()->projs();
                        DefVec pass_list;
                        for (auto pass : pass_array) { pass_list.push_back(pass); }
                        addPasses(world, builder, passes, pass_list);
                    };

                passes[flags_t(Axiom::Base<thorin::compile::phases_to_phase>)] =
                    [&](World& world, PipelineBuilder& builder, const Def* app) {
                        auto phase_array = app->as<App>()->arg()->projs();
                        DefVec phase_list;
                        for (auto phase : phase_array) { phase_list.push_back(phase); }
                        addPhases(phase_list, world, passes, builder);
                    };
            },
            nullptr, [](Normalizers& normalizers) { compile::register_normalizers(normalizers); }};
}
