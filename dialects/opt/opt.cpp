#include "dialects/opt/opt.h"

#include <string_view>

#include <thorin/driver.h>
#include <thorin/pass/pass.h>

#include "dialects/compile/compile.h"
#include "dialects/opt/autogen.h"

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"opt", nullptr,
            [](Passes& passes) {
                passes[flags_t(Annex::Base<compile::plugin_select>)] = [&](World& world, PipelineBuilder& builder,
                                                                           const Def* app) {
                    auto& driver      = builder.world().driver();
                    auto [ax, args]   = collect_args(app);
                    auto plugin_axiom = args[1]->as<Axiom>();
                    auto then_phase   = args[2];
                    auto else_phase   = args[3];
                    auto name         = plugin_axiom->sym();                        // name has the form %opt.tag
                    auto [_, tag, __] = Annex::split(world, name);                  // where tag = [plugin]_plugin
                    auto plugin       = tag.view().substr(0, tag.view().find('_')); // we want to extract the plugin
                    bool is_loaded    = driver.is_loaded(driver.sym(plugin));

                    assert(tag.view().find('_') != std::string_view::npos && "plugin_phase: invalid plugin name");
                    world.DLOG("plugin_phase for: {}", plugin_axiom->sym());
                    world.DLOG("plugin: {}", plugin);
                    world.DLOG("contained: {}", is_loaded);

                    compile::handle_optimization_part(is_loaded ? then_phase : else_phase, world, passes, builder);
                };
            },
            nullptr};
}
