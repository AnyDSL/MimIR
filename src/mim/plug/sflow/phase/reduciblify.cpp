#include "mim/phase.h"

#include "mim/plug/sflow/cfg.h"

namespace mim::plug::sflow::phase {

using namespace mim;

class Reduciblifier : public mim::RWPhase {
public:
    Reduciblifier(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    const Def* rewrite_mut_Lam(Lam* d) override {
        // Create a CFG of this lam and make it reducible.
        CFG cfg(d);
        reduciblify(cfg);

        return rewrite_stub(d, new_world().mut_lam(rewrite(d->type())->as<Pi>()));
    }

    const Def* rewrite_imm_App(const App* d) override {
        // TODO: change callee depending on transformed cfg
        // TODO: think of a way to keep track of exact occurrences of
        // a lam in the cfg, to it can be matched here
        return new_world().app(rewrite(d->callee()), rewrite(d->arg()));
    }

private:
    void reduciblify(CFG& cfg) {
        // Create limit graph
        CFG limit = cfg;
        limit.reduce();

        while (!limit.entry()->succs.empty()) {
            // TODO: split some node

            limit.reduce();
        }
    }
};

} // namespace mim::plug::sflow::phase
