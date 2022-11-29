#pragma once

#include <functional>
#include <vector>

#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/phase/phase.h"

namespace thorin {

using PassBuilder      = std::function<void(PassMan&)>;
using PhaseBuilder     = std::function<void(Pipeline&)>;
using PrioPassBuilder  = std::pair<int, PassBuilder>;
using PrioPhaseBuilder = std::pair<int, PhaseBuilder>;
using PassList         = std::vector<PrioPassBuilder>;
using PhaseList        = std::vector<PrioPhaseBuilder>;
using PassInstanceMap  = absl::btree_map<const Def*, Pass*, GIDLt<const Def*>>;

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
    void add_pass(const Def* def, Args&&... args) {
        append_pass_in_end([&, def, ... args = std::forward<Args>(args)](PassMan& man) {
            auto pass = (Pass*)man.add<P>(args...);
            remember_pass_instance(pass, def);
        });
    }

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

template<class A, class P, class... CArgs>
void register_pass(Passes& passes, CArgs&&... args) {
    passes[flags_t(Axiom::Base<A>)] = [... args = std::forward<CArgs>(args)](World&, PipelineBuilder& builder,
                                                                             const Def* app) {
        builder.add_pass<P>(app, args...);
    };
}

template<class A, class P, class Q>
void register_pass_with_arg(Passes& passes) {
    passes[flags_t(Axiom::Base<A>)] = [](World&, PipelineBuilder& builder, const Def* app) {
        auto pass_arg = (Q*)(builder.get_pass_instance(app->as<App>()->arg()));
        builder.add_pass<P>(app, pass_arg);
    };
}

} // namespace thorin
