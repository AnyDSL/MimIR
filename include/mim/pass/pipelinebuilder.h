#pragma once

#include "mim/pass/optimize.h"
#include "mim/pass/pass.h"
#include "mim/phase/phase.h"
#include "mim/plugin.h"
#include "mim/world.h"

namespace mim {

class PipelineBuilder {
public:
    PipelineBuilder(World& world)
        : pipe(std::make_unique<Pipeline>(world))
        , world_(world) {}

    World& world() { return world_; }

    // Adds a pass and remembers it associated with the given def.
    template<class P, class... Args> void add_pass(const Def* def, Args&&... args) {
        auto pass = (Pass*)man->add<P>(std::forward<Args>(args)...);
        def2pass(def, pass);
    }
    // TODO: add remembered entry
    template<class P, class... Args> void add_phase(Args&&... args) {
        assert(!man && "cannot add phase while in pass phase");
        pipe->add<P>(std::forward<Args>(args)...);
    }

    void begin_pass_phase();
    void end_pass_phase();

    void def2pass(const Def*, Pass* p);
    Pass* pass(const Def*);

    void run_pipeline();

private:
    absl::btree_map<const Def*, Pass*, GIDLt<const Def*>> def2pass_;
    std::unique_ptr<PassMan> man;
    std::unique_ptr<Pipeline> pipe;
    World& world_;
};

/// @name Register Pass/Phase
///@{
template<class A, class P, class... CArgs> void register_pass(Passes& passes, CArgs&&... args) {
    assert_emplace(passes, flags_t(Annex::Base<A>),
                   [... args = std::forward<CArgs>(args)](World&, PipelineBuilder& builder, const Def* app) {
                       builder.add_pass<P>(app, args...);
                   });
}

template<class A, class P, class... CArgs> void register_phase(Passes& passes, CArgs&&... args) {
    assert_emplace(passes, flags_t(Annex::Base<A>),
                   [... args = std::forward<CArgs>(args)](World&, PipelineBuilder& builder, const Def*) {
                       builder.add_phase<P>(args...);
                   });
}

template<class A, class P, class Q> void register_pass_with_arg(Passes& passes) {
    assert_emplace(passes, flags_t(Annex::Base<A>), [](World& world, PipelineBuilder& builder, const Def* app) {
        auto pass_arg = (Q*)(builder.pass(app->as<App>()->arg()));
        world.DLOG("register using arg: {} of type {} for gid {}", pass_arg, typeid(Q).name(),
                   app->as<App>()->arg()->gid());
        builder.add_pass<P>(app, pass_arg);
    });
}
///@}

} // namespace mim
