#include "mim/plug/opt/opt.h"

#include <string_view>

#include <mim/driver.h>

#include <mim/pass/pass.h>

#include "mim/plug/compile/compile.h"
#include "mim/plug/opt/autogen.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    auto passes = [](Passes& passes) {
        passes[flags_t(Annex::Base<compile::plugin_select>)]
            = [&](World& world, PipelineBuilder& builder, const Def* app) {
                  auto& driver      = builder.world().driver();
                  auto [ax, args]   = collect_args(app);
                  auto plugin_axm   = args[1]->as<Axm>();
                  auto then_phase   = args[2];
                  auto else_phase   = args[3];
                  auto name         = plugin_axm->sym();                          // name has the form %opt.tag
                  auto [_, tag, __] = Annex::split(driver, name);                 // where tag = [plugin]_plugin
                  auto plugin       = tag.view().substr(0, tag.view().find('_')); // we want to extract the plugin
                  bool is_loaded    = driver.is_loaded(driver.sym(plugin));

                  assert(tag.view().find('_') != std::string_view::npos && "mim/plugin_phase: invalid plugin name");
                  world.DLOG("mim/plugin_phase for: {}", plugin_axm->sym());
                  world.DLOG("mim/plugin: {}", plugin);
                  world.DLOG("contained: {}", is_loaded);

                  compile::handle_optimization_part(is_loaded ? then_phase : else_phase, world, passes, builder);
              };
    };

    return {"opt", nullptr, passes, nullptr};
}
