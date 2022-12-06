#pragma once

// #include <cstddef>

// #include <functional>
// #include <memory>
// #include <string>
// #include <vector>

// #include <map>

#include "thorin/world.h"

#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/phase/phase.h"

namespace thorin {

// using PassBuilder      = std::function<void(PassMan&)>;
// using PhaseBuilder     = std::function<void(Pipeline&)>;
// using PrioPassBuilder  = PassBuilder;
// using PrioPhaseBuilder = PhaseBuilder;
// using PassList         = std::vector<PrioPassBuilder>;
// using PhaseList        = std::vector<PrioPhaseBuilder>;
// using PassInstanceMap = absl::btree_map<const Def*, Pass*, GIDLt<const Def*>>;
using PassInstanceMap = std::map<u32, Pass*>;

class PipelineBuilder {
public:
    PipelineBuilder(World& world)
        : // pipe(world)
          // ,
        world_(world) {
        pipe = std::make_unique<Pipeline>(world);
        // man = std::make_unique<PassMan>(world);
        man = nullptr;
    }

    // int last_phase();

    // Adds a pass and remembers it associated with the given def.
    template<class P, class... Args>
    void add_pass(const Def* def, Args&&... args) {
        // append_pass_in_end([&, def, ... args = std::forward<Args>(args)](PassMan& man) {
        auto pass = (Pass*)man->add<P>(std::forward<Args>(args)...);
        remember_pass_instance(pass, def);
        // });
    }
    template<class P, class... Args>
    void add_phase(Args&&... args) {
        assert(!man && "cannot add phase while in pass phase");
        pipe->add<P>(std::forward<Args>(args)...);
    }

    void begin_pass_phase();
    void end_pass_phase();

    void remember_pass_instance(Pass* p, const Def*);
    Pass* get_pass_instance(const Def*);

    // void add_phase(Phase);
    // void add_pass(Pass);

    // void append_phase_end(PhaseBuilder);
    // void append_pass_in_end(PassBuilder);

    // void append_pass_after_end(PassBuilder);

    // void append_phase(int i, PhaseBuilder);
    // void extend_opt_phase(int i, PassBuilder);

    // std::unique_ptr<PassMan> opt_phase(int i, World& world);
    // void buildPipeline(Pipeline& pipeline);
    // void buildPipelinePart(int i, Pipeline& pipeline);
    // void add_opt(PassMan man);

    // std::vector<int> passes();
    // std::vector<int> phases();

    void register_dialect(Dialect& d);
    bool is_registered_dialect(std::string name);

    // Pipeline get_pipeline();
    void run_pipeline();

private:
    // std::map<int, PassList> pass_extensions_;
    // std::map<int, PhaseList> phase_extensions_;
    std::set<std::string> registered_dialects_;
    std::vector<Dialect*> dialects_;
    PassInstanceMap pass_instances_;
    std::unique_ptr<PassMan> man;
    std::unique_ptr<Pipeline> pipe;
    World& world_;
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
    passes[flags_t(Axiom::Base<A>)] = [](World& world, PipelineBuilder& builder, const Def* app) {
        auto pass_arg = (Q*)(builder.get_pass_instance(app->as<App>()->arg()));
        world.DLOG("register using arg: {} of type {} for gid {}", pass_arg, typeid(Q).name(),
                   app->as<App>()->arg()->gid());
        builder.add_pass<P>(app, pass_arg);
    };
}

} // namespace thorin
