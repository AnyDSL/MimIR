#include "thorin/pass/pipelinebuilder.h"

#include "thorin/def.h"
#include "thorin/lattice.h"
#include "thorin/plugin.h"

#include "thorin/pass/beta_red.h"
#include "thorin/pass/eta_exp.h"
#include "thorin/pass/eta_red.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/ret_wrap.h"
#include "thorin/pass/scalarize.h"
#include "thorin/pass/tail_rec_elim.h"
#include "thorin/phase/phase.h"

namespace thorin {

void PipelineBuilder::def2pass(const Def* def, Pass* p) {
    def->world().DLOG("associating {} with {}", def->gid(), p);
    def2pass_[def] = p;
}
Pass* PipelineBuilder::pass(const Def* def) { return def2pass_[def]; }

void PipelineBuilder::begin_pass_phase() { man = std::make_unique<PassMan>(world_); }
void PipelineBuilder::end_pass_phase() {
    std::unique_ptr<thorin::PassMan>&& pass_man_ref = std::move(man);
    pipe->add<PassManPhase>(std::move(pass_man_ref));
    man = nullptr;
}

void PipelineBuilder::run_pipeline() { pipe->run(); }

} // namespace thorin
