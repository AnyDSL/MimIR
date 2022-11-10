#pragma once

#include <functional>
#include <vector>

#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/phase/phase.h"

namespace thorin {

typedef std::function<void(PassMan&)> PassBuilder;
typedef std::function<void(Pipeline&)> PhaseBuilder;
typedef std::pair<int, PassBuilder> PrioPassBuilder;
typedef std::pair<int, PhaseBuilder> PrioPhaseBuilder;
typedef std::vector<PrioPassBuilder> PassList;
typedef std::vector<PrioPhaseBuilder> PhaseList;

struct passCmp {
    constexpr bool operator()(PrioPassBuilder const& a, PrioPassBuilder const& b) const noexcept {
        return a.first < b.first;
    }
};

struct phaseCmp {
    constexpr bool operator()(PrioPhaseBuilder const& a, PrioPhaseBuilder const& b) const noexcept {
        return a.first < b.first;
    }
};

class PipelineBuilder {
public:
    explicit PipelineBuilder() {}

    void append_phase(int i, PhaseBuilder, int priority = Pass_Default_Priority);
    void extend_opt_phase(int i, std::function<void(PassMan&)>, int priority = Pass_Default_Priority);
    void extend_opt_phase(std::function<void(PassMan&)>&&);
    void add_opt(int i);
    void extend_codegen_prep_phase(std::function<void(PassMan&)>&&);

    std::unique_ptr<PassMan> opt_phase(int i, World& world);
    void buildPipeline(Pipeline& pipeline);
    void buildPipelinePart(int i, Pipeline& pipeline);
    void add_opt(PassMan man);

    std::vector<int> passes();
    std::vector<int> phases();

private:
    std::map<int, PassList> pass_extensions_;
    std::map<int, PhaseList> phase_extensions_;
};

} // namespace thorin
