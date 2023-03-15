#include "dialects/opt/opt.h"

#include <string_view>

#include <thorin/driver.h>
#include <thorin/pass/pass.h>

#include "dialects/compile/compile.h"
#include "dialects/opt/autogen.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"opt",
            [](Passes& passes) {
                passes[flags_t(Axiom::Base<compile::dialect_select>)] = [&](World& world, PipelineBuilder& builder,
                                                                        const Def* app) {
                    auto [ax, args]    = collect_args(app);
                    auto dialect_axiom = args[1]->as<Axiom>();
                    auto then_phase    = args[2];
                    auto else_phase    = args[3];
                    world.DLOG("dialect_phase for: {}", dialect_axiom->sym());

                    // name has the form %opt.tag where tag = [dialect]_dialect
                    // we want to extract the dialect part
                    auto name         = dialect_axiom->sym();
                    auto [_, tag, __] = Axiom::split(world, name);
                    assert(tag->find('_') != std::string_view::npos && "dialect_phase: invalid dialect name");
                    auto dialect     = tag->substr(0, tag->find('_'));
                    auto dialect_str = std::string(dialect);
                    world.DLOG("dialect: {}", dialect_str);
                    auto& driver = builder.world().driver();
                    bool is_loaded = driver.sym2plugin(driver.sym(dialect_str));
                    world.DLOG("contained: {}", is_loaded);

                    compile::handle_optimization_part(is_loaded ? then_phase : else_phase, world, passes, builder);
                };
            },
            nullptr, nullptr};
}
