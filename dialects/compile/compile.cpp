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

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    // clang-format off
    return {"compile", 
        nullptr, 
        [](Passes& passes) { 
            auto debug_phase_flag = flags_t(Axiom::Base<thorin::compile::debug_phase>);
            // std::cout << "debug_phase_flags: " << debug_phase_flag << std::endl;
            passes[debug_phase_flag] = 
                [](World& world, PipelineBuilder& builder, const Def* app) {
                    world.DLOG("Generate debug_phase: {}", app);
                    int level = (int)(app->as<App>()->arg(0)->as<Lit>()->get<u64>());
                    world.DLOG("  Level: {}", level);
                    // TODO: add a debug pass to the pipeline
                    builder.append_pass_after_end(
                        [&](PassMan& man) {
                            man.add<thorin::compile::DebugPrint>(level);
                            // man.add<thorin::compile::DebugPrint>(1);
                        }
                    );
                } ;

            auto pass_phase_flag = flags_t(Axiom::Base<thorin::compile::pass_of_phase>);
            passes[pass_phase_flag] = 
                [](World& world, PipelineBuilder& builder, const Def* app) {
                    // auto [ax, pass_phase] = collect_args(app);
                    auto [ax,passes] = collect_args(app->as<App>()->arg());
                    auto last_phase = builder.last_phase();

                    // TODO: get passes from axioms (another passes array?)
                    // TODO: maybe everything one level lower (phase -> passman), pass -> ? (something that allows composing it here)
                } ;

        }, 
        nullptr,
        nullptr
    };
    // clang-format on
}
