#include "thorin/pass/pipelinebuilder.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/alloc2malloc.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/remem_elim.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

using namespace thorin;

void PipelineBuilder::extend_opt_phase(std::function<void(PassMan&)> extension) {
    opt_phase_extensions_.push_back(extension);
}

void PipelineBuilder::extend_codegen_prep_phase(std::function<void(PassMan&)> extension) {
    codegen_prep_phase_extensions_.push_back(extension);
}

PassMan PipelineBuilder::opt_phase(World& world) {
    PassMan man{world};

    man.add<PartialEval>();
    auto br = man.add<BetaRed>();
    auto er = man.add<EtaRed>();
    auto ee = man.add<EtaExp>(er);
    man.add<SSAConstr>(ee);
    man.add<Scalerize>(ee);
    man.add<CopyProp>(br, ee);
    man.add<TailRecElim>(er);

    for (const auto& ext : opt_phase_extensions_) ext(man);

    return man;
}

PassMan PipelineBuilder::codegen_prep_phase(World& world) {
    PassMan man{world};

    man.add<RememElim>();
    man.add<Alloc2Malloc>();
    man.add<RetWrap>();

    for (const auto& ext : codegen_prep_phase_extensions_) ext(man);

    return man;
}
