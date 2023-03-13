#pragma once

#include "thorin/world.h"

#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/phase/phase.h"

namespace thorin {

using PassInstanceMap = absl::btree_map<const Def*, Pass*, GIDLt<const Def*>>;

class PipelineBuilder {
public:
    PipelineBuilder(World& world)
        : pipe(std::make_unique<Pipeline>(world))
        , world_(world) {}

    // Adds a pass and remembers it associated with the given def.
    template<class P, class... Args>
    void add_pass(const Def* def, Args&&... args) {
        auto pass = (Pass*)man->add<P>(std::forward<Args>(args)...);
        remember_pass_instance(pass, def);
    }
    // TODO: add rememered entry
    template<class P, class... Args>
    void add_phase(Args&&... args) {
        assert(!man && "cannot add phase while in pass phase");
        pipe->add<P>(std::forward<Args>(args)...);
    }

    void begin_pass_phase();
    void end_pass_phase();

    void remember_pass_instance(Pass* p, const Def*);
    Pass* get_pass_instance(const Def*);

    void register_dialect(Dialect& d);
    bool is_registered_dialect(std::string name);

    void run_pipeline();

private:
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

template<class A, class P, class... CArgs>
void register_phase(Passes& passes, CArgs&&... args) {
    passes[flags_t(Axiom::Base<A>)] = [... args = std::forward<CArgs>(args)](World&, PipelineBuilder& builder,
                                                                             const Def* app) {
        builder.add_phase<P>(args...);
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
