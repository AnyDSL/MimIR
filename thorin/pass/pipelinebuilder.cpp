#include "thorin/pass/pipelinebuilder.h"

// #include <compare>

// #include <memory>
// #include <vector>

#include "thorin/def.h"
#include "thorin/dialects.h"
#include "thorin/lattice.h"

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

void PipelineBuilder::remember_pass_instance(Pass* p, const Def* def) {
    def->world().DLOG("associating {} with {}", def->gid(), p);
    pass_instances_[def] = p;
}
Pass* PipelineBuilder::get_pass_instance(const Def* def) { return pass_instances_[def]; }

void PipelineBuilder::begin_pass_phase() { man = std::make_unique<PassMan>(world_); }
void PipelineBuilder::end_pass_phase() {
    std::unique_ptr<thorin::PassMan>&& pass_man_ref = std::move(man);
    pipe->add<PassManPhase>(std::move(pass_man_ref));
    man = nullptr;
}

void PipelineBuilder::register_dialect(Dialect& dialect) { dialects_.push_back(&dialect); }
bool PipelineBuilder::is_registered_dialect(std::string name) {
    for (auto& dialect : dialects_) {
        if (dialect->name() == name) { return true; }
    }
    return false;
}

void PipelineBuilder::run_pipeline() { pipe->run(); }

} // namespace thorin
