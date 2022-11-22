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
using PassInstanceMap = absl::flat_hash_map<const Def*, Pass*>;

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

    int last_phase();

    // Adds a pass and remembers it associated with the given def.
    template<class P, class... Args>
    void add_pass(const Def*, Args&&...);
    void remember_pass_instance(Pass* p, const Def*);
    Pass* get_pass_instance(const Def*);
    void append_phase_end(PhaseBuilder, int priority = Pass_Default_Priority);
    void append_pass_in_end(PassBuilder, int priority = Pass_Default_Priority);

    void append_pass_after_end(PassBuilder, int priority = Pass_Default_Priority);

    void append_phase(int i, PhaseBuilder, int priority = Pass_Default_Priority);
    void extend_opt_phase(int i, PassBuilder, int priority = Pass_Default_Priority);
    void extend_opt_phase(PassBuilder&&);
    void add_opt(int i);
    void extend_codegen_prep_phase(PassBuilder&&);

    std::unique_ptr<PassMan> opt_phase(int i, World& world);
    void buildPipeline(Pipeline& pipeline);
    void buildPipelinePart(int i, Pipeline& pipeline);
    void add_opt(PassMan man);

    std::vector<int> passes();
    std::vector<int> phases();

private:
    std::map<int, PassList> pass_extensions_;
    std::map<int, PhaseList> phase_extensions_;
    PassInstanceMap pass_instances_;
};

} // namespace thorin
