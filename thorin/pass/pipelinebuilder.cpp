#include "thorin/pass/pipelinebuilder.h"

#include <compare>

#include <vector>

#include "thorin/def.h"
#include "thorin/lattice.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

#include "dialects/mem/passes/fp/copy_prop.h"
#include "dialects/mem/passes/fp/ssa_constr.h"
#include "dialects/mem/passes/rw/alloc2malloc.h"
#include "dialects/mem/passes/rw/remem_elim.h"

namespace thorin {

void PipelineBuilder::remember_pass_instance(Pass* p, const Def* def) { pass_instances_[def] = p; }
Pass* PipelineBuilder::get_pass_instance(const Def* def) { return pass_instances_[def]; }

int PipelineBuilder::last_phase() {
    auto phase_ids    = phases();
    auto max_phase_id = std::max_element(phase_ids.begin(), phase_ids.end());
    auto max_phase    = max_phase_id == phase_ids.end() ? 0 : *max_phase_id;
    return max_phase;
}

void PipelineBuilder::append_phase_end(PhaseBuilder phase, int priority) {
    append_phase(last_phase() + 1, phase, priority);
}
void PipelineBuilder::append_pass_in_end(PassBuilder pass, int priority) {
    extend_opt_phase(last_phase(), pass, priority);
}
void PipelineBuilder::append_pass_after_end(PassBuilder pass, int priority) {
    extend_opt_phase(last_phase() + 1, pass, priority);
}

void PipelineBuilder::extend_opt_phase(PassBuilder&& extension) { extend_opt_phase(Opt_Phase, extension); }

void PipelineBuilder::extend_codegen_prep_phase(PassBuilder&& extension) {
    extend_opt_phase(Codegen_Prep_Phase, extension);
}

void PipelineBuilder::append_phase(int i, PhaseBuilder extension, int priority) {
    if (!phase_extensions_.contains(i)) { phase_extensions_[i] = PhaseList(); }
    phase_extensions_[i].push_back({priority, extension});
}

void PipelineBuilder::extend_opt_phase(int i, PassBuilder extension, int priority) {
    // adds extension to the i-th optimization phase
    // if the ith phase does not exist, it is created
    if (!pass_extensions_.contains(i)) { pass_extensions_[i] = std::vector<PrioPassBuilder>(); }
    pass_extensions_[i].push_back({priority, extension});
}

void PipelineBuilder::add_opt(int i) {
    extend_opt_phase(
        i,
        [](thorin::PassMan& man) {
            man.add<PartialEval>();
            man.add<BetaRed>();
            auto er = man.add<EtaRed>();
            auto ee = man.add<EtaExp>(er);
            man.add<Scalerize>(ee);
            man.add<TailRecElim>(er);
        },
        Pass_Internal_Priority); // elevated priority
}

std::vector<int> PipelineBuilder::passes() {
    std::vector<int> keys;
    for (auto iter = pass_extensions_.begin(); iter != pass_extensions_.end(); iter++) { keys.push_back(iter->first); }
    std::ranges::stable_sort(keys);
    return keys;
}

std::vector<int> PipelineBuilder::phases() {
    std::vector<int> keys;
    for (auto iter = pass_extensions_.begin(); iter != pass_extensions_.end(); iter++) { keys.push_back(iter->first); }
    for (auto iter = phase_extensions_.begin(); iter != phase_extensions_.end(); iter++) {
        keys.push_back(iter->first);
    }
    std::ranges::stable_sort(keys);
    // erase duplicates to avoid duplicating phases
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

std::unique_ptr<PassMan> PipelineBuilder::opt_phase(int i, World& world) {
    auto man = std::make_unique<PassMan>(world);

    std::stable_sort(pass_extensions_[i].begin(), pass_extensions_[i].end(), passCmp());

    for (const auto& ext : pass_extensions_[i]) { ext.second(*man); }

    return man;
}

void PipelineBuilder::buildPipeline(Pipeline& pipeline) {
    for (auto i : phases()) { buildPipelinePart(i, pipeline); }
}
void PipelineBuilder::buildPipelinePart(int i, Pipeline& pipeline) {
    if (pass_extensions_.contains(i)) { pipeline.add<PassManPhase>(opt_phase(i, pipeline.world())); }

    if (phase_extensions_.contains(i)) {
        std::stable_sort(phase_extensions_[i].begin(), phase_extensions_[i].end(), phaseCmp());
        for (const auto& ext : phase_extensions_[i]) { ext.second(pipeline); }
    }
}

} // namespace thorin
