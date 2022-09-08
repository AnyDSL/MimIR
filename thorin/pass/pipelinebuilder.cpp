#include "thorin/pass/pipelinebuilder.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/phase/phase.h"

#include "dialects/mem/passes/fp/copy_prop.h"
#include "dialects/mem/passes/fp/ssa_constr.h"
#include "dialects/mem/passes/rw/alloc2malloc.h"
#include "dialects/mem/passes/rw/remem_elim.h"

namespace thorin {

void PipelineBuilder::extend_opt_phase(std::function<void(PassMan&)> extension) {
    opt_phase_extensions_.push_back(extension);
}

void PipelineBuilder::extend_codegen_prep_phase(std::function<void(PassMan&)> extension) {
    codegen_prep_phase_extensions_.push_back(extension);
}

void PipelineBuilder::opt_phase(World& world, Pipeline& pipeline) {
    auto man = std::make_unique<PassMan>(world);
    man->add<PartialEval>();
    man->add<BetaRed>();
    auto er = man->add<EtaRed>();
    auto ee = man->add<EtaExp>(er);
    man->add<Scalerize>(ee);
    man->add<TailRecElim>(er);
    pipeline.add<PassManPhase>(std::move(man));

    for (const auto& ext : opt_phase_extensions_){
        auto man = std::make_unique<PassMan>(world);
        ext(*man);
        pipeline.add<PassManPhase>(std::move(man));
    }
}

std::unique_ptr<PassMan> PipelineBuilder::codegen_prep_phase(World& world) {
    auto man = std::make_unique<PassMan>(world);

    man->add<RetWrap>();

    for (const auto& ext : codegen_prep_phase_extensions_) ext(*man);

    return man;
}

} // namespace thorin
