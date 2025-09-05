#include "mim/plug/compile/compile.h"

#include <memory>
#include <type_traits>

#include <mim/config.h>
#include <mim/driver.h>
#include <mim/pass.h>
#include <mim/phase.h>

#include <mim/pass/beta_red.h>
#include <mim/pass/eta_exp.h>
#include <mim/pass/eta_red.h>
#include <mim/pass/lam_spec.h>
#include <mim/pass/ret_wrap.h>
#include <mim/pass/scalarize.h>
#include <mim/pass/tail_rec_elim.h>
#include <mim/phase/beta_red_phase.h>
#include <mim/phase/branch_normalize.h>
#include <mim/phase/eta_exp_phase.h>
#include <mim/phase/eta_red_phase.h>
#include <mim/phase/prefix_cleanup.h>

using namespace mim;
using namespace mim::plug;

template<class P, class M>
auto apply(World& world, P& ps, M& man, const Def* app) {
    auto p_def = App::uncurry_callee(app);
    world.DLOG("apply pass/phase: `{}`", p_def);

    if (auto axm = p_def->isa<Axm>())
        if (auto i = ps.find(axm->flags()); i != ps.end())
            if constexpr (std::is_same_v<M, PhaseMan>)
                return i->second(world, app);
            else
                return i->second(man, app);
        else
            error("pass/phase `{}` not found", axm->sym());
    else
        error("unsupported callee for a phase/pass: `{}`", p_def);
}

void reg_stages(Flags2Phases& phases, Flags2Passes& passes) {
    // clang-format off
    assert_emplace(phases, Annex::Base<compile::null_phase>, [](World&,    const Def*) { return std::unique_ptr<Phase>{}; });
    assert_emplace(passes, Annex::Base<compile::null_pass >, [](PassMan&,  const Def*) {});

    PhaseMan::hook<compile::cleanup_phase,  Cleanup     >(phases);
    PhaseMan::hook<compile::beta_red_phase, BetaRedPhase>(phases);
    PhaseMan::hook<compile::eta_red_phase,  EtaRedPhase >(phases);
    PhaseMan::hook<compile::eta_exp_phase,  EtaExpPhase >(phases);
    // clang-format on

    assert_emplace(phases, Annex::Base<compile::prefix_cleanup_phase>, [&](World& world, const Def* app) {
        auto prefix = tuple2str(app->as<App>()->arg());
        return std::make_unique<PrefixCleanup>(world, prefix);
    });

    assert_emplace(phases, Annex::Base<compile::passes>, [&](World& world, const Def* app) {
        auto defs = app->as<App>()->arg()->projs();
        auto man  = std::make_unique<PassMan>(app->world());
        for (auto def : defs)
            apply(man->world(), passes, *man, def);
        return std::make_unique<PassManPhase>(world, std::move(man));
    });

    assert_emplace(phases, Annex::Base<compile::phases>, [&](World& world, const Def* app) {
        auto [fp, arg] = App::uncurry_args<2>(app);
        auto man       = std::make_unique<PhaseMan>(world, Lit::as<bool>(fp));
        for (auto def : app->as<App>()->arg()->projs())
            if (auto phase = apply(world, phases, *man, def)) man->add(std::move(phase));
        return man;
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

    assert_emplace(passes, Annex::Base<compile::meta_pass>, [&](PassMan& man, const Def* app) {
        for (auto def : app->as<App>()->arg()->projs())
            apply(man.world(), passes, man, def);
    });
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"compile", compile::register_normalizers, reg_stages, nullptr};
}
