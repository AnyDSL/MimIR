#include "mim/plug/compile/compile.h"

#include <memory>

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

#include "mim/plug/compile/autogen.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Flags2Phases& phases, Flags2Passes& passes) {
    // clang-format off
    assert_emplace(phases, Annex::base<compile::null_phase>(), [](World&) { return std::unique_ptr<Phase>{}; });
    assert_emplace(passes, Annex::base<compile::null_pass >(), [](World&) { return std::unique_ptr<Pass >{}; });

    PhaseMan::hook<compile::beta_red_phase,         BetaRedPhase        >(phases);
    PhaseMan::hook<compile::branch_normalize_phase, BranchNormalizePhase>(phases);
    PhaseMan::hook<compile::cleanup_phase,          Cleanup             >(phases);
    PhaseMan::hook<compile::eta_exp_phase,          EtaExpPhase         >(phases);
    PhaseMan::hook<compile::eta_red_phase,          EtaRedPhase         >(phases);
    PhaseMan::hook<compile::passes,                 PassManPhase        >(phases);
    PhaseMan::hook<compile::phases,                 PhaseMan            >(phases);
    PhaseMan::hook<compile::prefix_cleanup_phase,   PrefixCleanup       >(phases);

    PassMan::hook<compile::beta_red_pass,      BetaRed    >(passes);
    PassMan::hook<compile::eta_exp_pass,       EtaExp     >(passes);
    PassMan::hook<compile::eta_red_pass,       EtaRed     >(passes);
    PassMan::hook<compile::lam_spec_pass,      LamSpec    >(passes);
    PassMan::hook<compile::ret_wrap_pass,      RetWrap    >(passes);
    PassMan::hook<compile::scalarize_pass,     Scalarize  >(passes);
    PassMan::hook<compile::tail_rec_elim_pass, TailRecElim>(passes);
    PassMan::hook<compile::meta_pass,          PassMan    >(passes);
    // clang-format on
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"compile", compile::register_normalizers, reg_stages, nullptr};
}
