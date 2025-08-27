#include "mim/plug/opt/opt.h"

#include <mim/driver.h>

#include <mim/phase/phase.h>

#include "mim/plug/compile/compile.h"
#include "mim/plug/opt/autogen.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Phases& phases, Passes& passes) {
    phases[flags_t(Annex::Base<compile::plugin_select>)] = [&](Pipeline& pipe, const Def* app) {
        auto& world        = pipe.world();
        auto& driver       = world.driver();
        auto [axm, tt, ff] = app->as<App>()->uncurry<3>(app);

        auto name         = axm->sym();                                 // name has the form %opt.tag
        auto [_, tag, __] = Annex::split(driver, name);                 // where tag = [plugin]_plugin
        auto plugin       = tag.view().substr(0, tag.view().find('_')); // we want to extract the plugin
        bool is_loaded    = driver.is_loaded(driver.sym(plugin));

        assert(tag.view().find('_') != std::string_view::npos && "mim/plugin_phase: invalid plugin name");
        world.DLOG("mim/plugin_phase for: {}", axm->sym());
        world.DLOG("mim/plugin: {}", plugin);
        world.DLOG("contained: {}", is_loaded);

        compile::apply(phases, pipe, is_loaded ? tt : ff);
    };

    passes[flags_t(Annex::Base<compile::plugin_select>)] = [&](PassMan& man, const Def* app) {
        auto& world        = man.world();
        auto& driver       = world.driver();
        auto [axm, tt, ff] = app->as<App>()->uncurry<3>(app);

        auto name         = axm->sym();                                 // name has the form %opt.tag
        auto [_, tag, __] = Annex::split(driver, name);                 // where tag = [plugin]_plugin
        auto plugin       = tag.view().substr(0, tag.view().find('_')); // we want to extract the plugin
        bool is_loaded    = driver.is_loaded(driver.sym(plugin));

        assert(tag.view().find('_') != std::string_view::npos && "mim/plugin_phase: invalid plugin name");
        world.DLOG("mim/plugin_phase for: {}", axm->sym());
        world.DLOG("mim/plugin: {}", plugin);
        world.DLOG("contained: {}", is_loaded);

        compile::apply(passes, man, is_loaded ? tt : ff);
    };
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"opt", nullptr, reg_stages, nullptr}; }
