#include "dialects/opt/opt.h"

#include <string_view>

#include <thorin/driver.h>
#include <thorin/pass/pass.h>

#include "dialects/compile/compile.h"
#include "dialects/opt/autogen.h"

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"opt",
            [](Passes& passes) {
                passes[flags_t(Axiom::Base<compile::dialect_select>)] = [&](World& world, PipelineBuilder& builder,
                                                                            const Def* app) {
                    auto& driver       = builder.world().driver();
                    auto [ax, args]    = collect_args(app);
                    auto dialect_axiom = args[1]->as<Axiom>();
                    auto then_phase    = args[2];
                    auto else_phase    = args[3];
                    auto name          = dialect_axiom->sym();                       // name has the form %opt.tag
                    auto [_, tag, __]  = Axiom::split(world, name);                  // where tag = [dialect]_dialect
                    auto dialect       = driver.sym(tag->substr(0, tag->find('_'))); // we want to extract the dialect
                    bool is_loaded     = driver.is_loaded(dialect);

                    assert(tag->find('_') != std::string_view::npos && "dialect_phase: invalid dialect name");
                    world.DLOG("dialect_phase for: {}", dialect_axiom->sym());
                    world.DLOG("dialect: {}", dialect);
                    world.DLOG("contained: {}", is_loaded);

                    compile::handle_optimization_part(is_loaded ? then_phase : else_phase, world, passes, builder);
                };
            },
            nullptr, nullptr};
}
