#include <memory>

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
    switch (id) {
            // clang-format off
        case phase::null:             return arg->world().lit(t, 0);
        case phase::beta_red:         return create<BetaRedPhase   >(id, t);
        case phase::branch_normalize: return create<BranchNormalize>(id, t);
        case phase::cleanup:          return create<Cleanup        >(id, t);
        case phase::eta_exp:          return create<EtaExpPhase    >(id, t);
        case phase::eta_red:          return create<EtaRedPhase    >(id, t);
        case phase::prefix_cleanup:   return create<PrefixCleanup  >(id, t, tuple2str(arg));
            // clang-format on
        case phase::from_pass:
            if (auto l = Lit::isa<::mim::Pass*>(arg)) {
                if (auto pass = *l) {
                    if (auto man = pass->isa<PassMan>())
                        return create<PassManPhase>(id, t, std::unique_ptr<PassMan>(man));
                    return create<PassManPhase>(id, t, std::unique_ptr<::mim::Pass>(pass));
                }
            }
            return arg->world().lit(t, 0);
        case phase::man: {
            auto fp     = callee->as<App>()->arg();
            auto phases = Phases();
            for (auto phase : arg->projs())
                if (auto p = ::mim::Phase::make_unique(phase)) phases.emplace_back(std::move(p));
            return create<PhaseMan>(id, t, Lit::get<bool>(fp), std::move(phases));
        }
    }
}

template<pass id>
const Def* normalize_pass(const Def* t, const Def*, const Def* arg) {
    switch (id) {
            // clang-format off
        case pass::null:          return arg->world().lit(t, 0);
        case pass::beta_red:      return create<BetaRed    >(id, t);
        case pass::lam_spec:      return create<LamSpec    >(id, t);
        case pass::ret_wrap:      return create<RetWrap    >(id, t);
        case pass::eta_red:       return create<EtaRed     >(id, t, Lit::get<bool   >(arg));
        case pass::eta_exp:       return create<EtaExp     >(id, t, Lit::get<EtaRed*>(arg));
        case pass::scalarize:     return create<Scalarize  >(id, t, Lit::get<EtaExp*>(arg));
        case pass::tail_rec_elim: return create<TailRecElim>(id, t, Lit::get<EtaRed*>(arg));
            // clang-format on
        case pass::man: {
            auto passes = Passes();
            for (auto pass : arg->projs())
                if (auto p = ::mim::Pass::make_unique(pass)) passes.emplace_back(std::move(p));
            return create<PassMan>(id, t, std::move(passes));
        }
    }
}

MIM_compile_NORMALIZER_IMPL

} // namespace mim::plug::compile
