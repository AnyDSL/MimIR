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
#include <mim/phase/beta_red_phase.h>
#include <mim/phase/branch_normalize.h>
#include <mim/phase/eta_exp_phase.h>
#include <mim/phase/eta_red_phase.h>
#include <mim/phase/phase.h>
#include <mim/phase/prefix_cleanup.h>

#include "mim/plug/compile/pass/debug_print.h"

using namespace mim;
using namespace mim::plug;

template<class P, class B>
void apply(P& ps, B& builder, const Def* app) {
    auto& world = app->world();
    auto p_def  = App::uncurry_callee(app);
    world.DLOG("apply pass/phase: `{}`", p_def);

    if (auto axm = p_def->isa<Axm>())
        if (auto i = ps.find(axm->flags()); i != ps.end())
            i->second(builder, app);
        else
            world.ELOG("pass/phase `{}` not found", axm->sym());
    else
        world.ELOG("unsupported callee for a phase/pass: `{}`", p_def);
}

void reg_stages(Phases& phases, Passes& passes) {
    // clang-format off
    assert_emplace(phases, flags_t(Annex::Base<compile::null_phase>), [](Pipeline&, const Def*) {});
    assert_emplace(passes, flags_t(Annex::Base<compile::null_pass >), [](PassMan&,  const Def*) {});

    Pipeline::hook<compile::cleanup_phase,  Cleanup     >(phases);
    Pipeline::hook<compile::beta_red_phase, BetaRedPhase>(phases);
    Pipeline::hook<compile::eta_red_phase,  EtaRedPhase >(phases);
    Pipeline::hook<compile::eta_exp_phase,  EtaExpPhase >(phases);
    // clang-format off

    assert_emplace(phases, flags_t(Annex::Base<compile::debug_phase>), [](Pipeline& pipe, const Def* app) {
        auto& world = pipe.world();
        world.DLOG("Generate debug_phase: {}", app);
        int level = (int)(app->as<App>()->arg(0)->as<Lit>()->get<u64>());
        world.DLOG("  Level: {}", level);
        pipe.add<compile::DebugPrint>(level);
    });

    assert_emplace(phases, flags_t(Annex::Base<compile::prefix_cleanup_phase>), [&](Pipeline& pipe, const Def* app) {
        auto prefix = tuple2str(app->as<App>()->arg());
        pipe.add<PrefixCleanup>(prefix);
    });

    assert_emplace(phases, flags_t(Annex::Base<compile::passes_to_phase>), [&](Pipeline& pipe, const Def* app) {
        auto defs = app->as<App>()->arg()->projs();
        auto man  = std::make_unique<PassMan>(app->world());
        for (auto def : defs)
            apply(passes, *man, def);
        pipe.add<PassManPhase>(std::move(man));
    });

    assert_emplace(phases, flags_t(Annex::Base<compile::phases_to_phase>), [&](Pipeline& pipe, const Def* app) {
        for (auto def : app->as<App>()->arg()->projs())
            apply(phases, pipe, def);
    });

    assert_emplace(phases, flags_t(Annex::Base<compile::pipeline>), [&](Pipeline& pipe, const Def* app) {
        auto [_, defs] = App::uncurry(app);
        for (auto def : defs)
            apply(phases, pipe, def);
    });

    // clang-format off
    PassMan::hook<compile::beta_red_pass,      BetaRed    >(passes);
    PassMan::hook<compile::eta_red_pass,       EtaRed     >(passes);
    PassMan::hook<compile::lam_spec_pass,      LamSpec    >(passes);
    PassMan::hook<compile::ret_wrap_pass,      RetWrap    >(passes);
    PassMan::hook<compile::eta_exp_pass,       EtaExp     >(passes);
    PassMan::hook<compile::scalarize_pass,     Scalarize  >(passes);
    PassMan::hook<compile::tail_rec_elim_pass, TailRecElim>(passes);
    // clang-format on
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"compile", compile::register_normalizers, reg_stages, nullptr};
}
