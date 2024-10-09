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

void PipelineBuilder::def2pass(const Def* def, Pass* p) {
    def->world().DLOG("associating {} with {}", def->gid(), p);
    def2pass_[def] = p;
}
Pass* PipelineBuilder::pass(const Def* def) { return def2pass_[def]; }

void PipelineBuilder::begin_pass_phase() { man = std::make_unique<PassMan>(world_); }
void PipelineBuilder::end_pass_phase() {
    std::unique_ptr<mim::PassMan>&& pass_man_ref = std::move(man);
    pipe->add<PassManPhase>(std::move(pass_man_ref));
    man = nullptr;
}

void PipelineBuilder::run_pipeline() { pipe->run(); }

} // namespace mim
