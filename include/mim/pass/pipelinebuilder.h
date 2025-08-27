#pragma once

#include "mim/plugin.h"
#include "mim/world.h"

#include "mim/pass/optimize.h"
#include "mim/pass/pass.h"
#include "mim/phase/phase.h"

namespace mim {

class PipelineBuilder {
public:
    PipelineBuilder(World& world)
        : pipe_(std::make_unique<Pipeline>(world))
        , world_(world) {}

    World& world() { return world_; }

    // Adds a pass and remembers it associated with the given def.
    template<class P, class... Args>
    void add_pass(Args&&... args) {
        man_->add<P>(std::forward<Args>(args)...);
    }

    template<class P, class... Args>
    void add_phase(Args&&... args) {
        assert(!man_ && "cannot add phase while in pass phase");
        pipe_->add<P>(std::forward<Args>(args)...);
    }

    void begin_pass_phase();
    void end_pass_phase();
    auto& man() { return *man_.get(); }

    void run_pipeline();

private:
    std::unique_ptr<PassMan> man_;
    std::unique_ptr<Pipeline> pipe_;
    World& world_;
};

/// @name Register Pass/Phase
///@{
template<class A, class P, class... CArgs>
void register_pass(Passes& passes, CArgs&&... args) {
    assert_emplace(passes, flags_t(Annex::Base<A>),
                   [... args = std::forward<CArgs>(args)](World&, PipelineBuilder& builder, const Def*) {
                       builder.add_pass<P>(args...);
                   });
}

template<class A, class P, class... CArgs>
void register_phase(Passes& passes, CArgs&&... args) {
    assert_emplace(passes, flags_t(Annex::Base<A>),
                   [... args = std::forward<CArgs>(args)](World&, PipelineBuilder& builder, const Def*) {
                       builder.add_phase<P>(args...);
                   });
}
///@}

} // namespace mim
