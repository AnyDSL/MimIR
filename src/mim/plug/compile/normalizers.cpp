#include <mim/driver.h>
#include <mim/world.h>

#include "mim/pass/beta_red.h"
#include "mim/pass/eta_exp.h"
#include "mim/pass/eta_red.h"
#include "mim/pass/lam_spec.h"
#include "mim/pass/ret_wrap.h"
#include "mim/pass/scalarize.h"
#include "mim/pass/tail_rec_elim.h"
#include "mim/phase/beta_red_phase.h"
#include "mim/phase/branch_normalize.h"
#include "mim/phase/eta_exp_phase.h"
#include "mim/phase/eta_red_phase.h"
#include "mim/phase/prefix_cleanup.h"

#include "mim/plug/compile/compile.h"

namespace mim::plug::compile {

const Def* normalize_is_loaded(const Def*, const Def*, const Def* arg) {
    auto& world  = arg->world();
    auto& driver = world.driver();
    if (auto str = tuple2str(arg); !str.empty()) return world.lit_bool(driver.is_loaded(driver.sym(str)));

    return {};
}

template<phase id>
const Def* normalize_phase(const Def* t, const Def* callee, const Def* arg) {
    auto& d = t->driver();

    switch (id) {
            // clang-format off
        case phase::null:             return arg->world().lit(t, 0);
        case phase::beta_red:         return d.stage_lit<BetaRedPhase   >(id, t);
        case phase::branch_normalize: return d.stage_lit<BranchNormalize>(id, t);
        case phase::cleanup:          return d.stage_lit<Cleanup        >(id, t);
        case phase::eta_exp:          return d.stage_lit<EtaExpPhase    >(id, t);
        case phase::eta_red:          return d.stage_lit<EtaRedPhase    >(id, t);
        case phase::prefix_cleanup:   return d.stage_lit<PrefixCleanup  >(id, t, tuple2str(arg));
            // clang-format on
        case phase::from_pass:
            if (auto l = Lit::isa<::mim::Pass*>(arg)) {
                if (auto pass = *l) {
                    if (auto man = pass->isa<PassMan>()) return d.stage_lit<PassManPhase>(id, t, d.own<PassMan>(man));
                    return d.stage_lit<PassManPhase>(id, t, d.own<::mim::Pass>(pass));
                }
            }
            return arg->world().lit(t, 0);
        case phase::man: {
            auto fp     = callee->as<App>()->arg();
            auto phases = Phases();
            for (auto a : arg->projs()) {
                if (auto l = Lit::isa<Stage*>(a)) {
                    if (auto s = *l) phases.emplace_back(d.own<::mim::Phase>(s));
                }
            }
            return d.stage_lit<PhaseMan>(id, t, Lit::get<bool>(fp), std::move(phases));
        }
    }
}

template<pass id>
const Def* normalize_pass(const Def* t, const Def*, const Def* arg) {
    auto& d = t->driver();
    switch (id) {
            // clang-format off
        case pass::null:          return arg->world().lit(t, 0);
        case pass::beta_red:      return d.stage_lit<BetaRed    >(id, t);
        case pass::lam_spec:      return d.stage_lit<LamSpec    >(id, t);
        case pass::ret_wrap:      return d.stage_lit<RetWrap    >(id, t);
        case pass::eta_red:       return d.stage_lit<EtaRed     >(id, t, Lit::get<bool   >(arg));
        case pass::eta_exp:       return d.stage_lit<EtaExp     >(id, t, Lit::get<EtaRed*>(arg));
        case pass::scalarize:     return d.stage_lit<Scalarize  >(id, t, Lit::get<EtaExp*>(arg));
        case pass::tail_rec_elim: return d.stage_lit<TailRecElim>(id, t, Lit::get<EtaRed*>(arg));
            // clang-format on
        case pass::man: {
            auto passes = Passes();
            for (auto a : arg->projs()) {
                if (auto l = Lit::isa<Stage*>(a)) {
                    if (auto s = *l) passes.emplace_back(d.own<::mim::Pass>(s));
                }
            }
            return d.stage_lit<PassMan>(id, t, std::move(passes));
        }
    }
}

MIM_compile_NORMALIZER_IMPL

} // namespace mim::plug::compile
