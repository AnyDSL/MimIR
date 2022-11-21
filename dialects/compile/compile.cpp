#include "dialects/compile/compile.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

#include "dialects/compile/passes/debug_print.h"

using namespace thorin;

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
                }
            ;
        }, 
        nullptr,
        nullptr
    };
    // clang-format on
}
