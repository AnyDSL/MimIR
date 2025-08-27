#include "mim/pass/pipelinebuilder.h"

#include "mim/def.h"
#include "mim/lattice.h"
#include "mim/plugin.h"

#include "mim/pass/beta_red.h"
#include "mim/pass/eta_exp.h"
#include "mim/pass/eta_red.h"
#include "mim/pass/pass.h"
#include "mim/pass/ret_wrap.h"
#include "mim/pass/scalarize.h"
#include "mim/pass/tail_rec_elim.h"
#include "mim/phase/phase.h"

namespace mim {

void PipelineBuilder::begin_pass_phase() { man_ = std::make_unique<PassMan>(world_); }
void PipelineBuilder::end_pass_phase() {
    std::unique_ptr<mim::PassMan>&& pass_man_ref = std::move(man_);
    pipe_->add<PassManPhase>(std::move(pass_man_ref));
    man_ = nullptr;
}

void PipelineBuilder::run_pipeline() { pipe_->run(); }

} // namespace mim
