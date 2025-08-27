#include "mim/plug/compile/compile.h"

#include <mim/config.h>
#include <mim/driver.h>

#include <mim/pass/beta_red.h>
#include <mim/pass/eta_exp.h>
#include <mim/pass/eta_red.h>
#include <mim/pass/lam_spec.h>
#include <mim/pass/pass.h>
#include <mim/pass/ret_wrap.h>
#include <mim/pass/scalarize.h>
#include <mim/pass/tail_rec_elim.h>
#include <mim/phase/phase.h>

#include "mim/plug/compile/pass/debug_print.h"
#include "mim/plug/compile/pass/internal_cleanup.h"

using namespace mim;
using namespace mim::plug;

void add_phases(Phases& phases, Pipeline& pipe, Defs defs) {
    for (auto def : defs)
        compile::apply(phases, pipe, def);
}

void reg_stages(Phases& phases, Passes& passes) {
    // clang-format off
    assert_emplace(phases, flags_t(Annex::Base<mim::plug::compile::null_phase>), [](Pipeline&, const Def*) {});
    assert_emplace(passes, flags_t(Annex::Base<mim::plug::compile::null_pass >), [](PassMan&,  const Def*) {});
    // clang-format on

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

    auto debug_phase_flag = flags_t(Annex::Base<mim::plug::compile::debug_phase>);
    assert_emplace(phases, debug_phase_flag, [](Pipeline& pipe, const Def* app) {
        auto& world = pipe.world();
        world.DLOG("Generate debug_phase: {}", app);
        int level = (int)(app->as<App>()->arg(0)->as<Lit>()->get<u64>());
        world.DLOG("  Level: {}", level);
        pipe.add<compile::DebugPrint>(level);
    });

    assert_emplace(phases, flags_t(Annex::Base<mim::plug::compile::passes_to_phase>),
                   [&](Pipeline& pipe, const Def* app) {
                       auto defs = app->as<App>()->arg()->projs();
                       auto man  = std::make_unique<PassMan>(app->world());
                       for (auto def : defs)
                           compile::apply(passes, *man, def);
                       pipe.add<PassManPhase>(std::move(man));
                   });

    assert_emplace(phases, flags_t(Annex::Base<mim::plug::compile::phases_to_phase>),
                   [&](Pipeline& pipe, const Def* app) {
                       auto phase_array = app->as<App>()->arg()->projs();
                       DefVec phase_list;
                       for (auto phase : phase_array)
                           phase_list.push_back(phase);

                       add_phases(phases, pipe, phase_list);
                   });

    assert_emplace(phases, flags_t(Annex::Base<mim::plug::compile::pipe>), [&](Pipeline& pipe, const Def* app) {
        auto [ax, defs] = App::uncurry(app);
        add_phases(phases, pipe, defs);
    });

    using compile::InternalCleanup;
    // clang-format off
    PassMan::hook<compile::beta_red_pass,          BetaRed        >(passes);
    PassMan::hook<compile::eta_red_pass,           EtaRed         >(passes);
    PassMan::hook<compile::lam_spec_pass,          LamSpec        >(passes);
    PassMan::hook<compile::ret_wrap_pass,          RetWrap        >(passes);
    PassMan::hook<compile::internal_cleanup_pass,  InternalCleanup>(passes);
    PassMan::hook<compile::eta_exp_pass,           EtaExp         >(passes);
    PassMan::hook<compile::scalarize_pass,         Scalarize      >(passes);
    PassMan::hook<compile::tail_rec_elim_pass,     TailRecElim    >(passes);
    // clang-format on
}

extern "C" MIM_EXPORT mim::Plugin mim_get_plugin() {
    return {"compile", [](Normalizers& n) { compile::register_normalizers(n); }, reg_stages, nullptr};
}
