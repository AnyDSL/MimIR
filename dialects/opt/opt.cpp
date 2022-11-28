#include "dialects/opt/opt.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

#include "dialects/compile/compile.h"
#include "dialects/opt/autogen.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"opt", nullptr,
            [](Passes& passes) {
                passes[flags_t(Axiom::Base<opt::dialect_phase>)] = [&](World& world, PipelineBuilder& builder,
                                                                       const Def* app) {
                    auto [dialect, then_phase, else_phase] = app->as<App>()->args<3>();
                    auto is_loaded                         = false;
                    if (is_loaded) {
                        compile::handle_optimization_part(then_phase, world, passes, builder);
                    } else {
                        compile::handle_optimization_part(else_phase, world, passes, builder);
                    }
                };
            },
            nullptr, [](Normalizers& normalizers) { opt::register_normalizers(normalizers); }};
}
