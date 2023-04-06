#include "thorin/pass/pipelinebuilder.h"

#include "thorin/def.h"
#include "thorin/lattice.h"
#include "thorin/plugin.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/phase/phase.h"

#include "dialects/mem/passes/fp/copy_prop.h"
#include "dialects/mem/passes/fp/ssa_constr.h"
#include "dialects/mem/passes/rw/alloc2malloc.h"
#include "dialects/mem/passes/rw/remem_elim.h"

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
