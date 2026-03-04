#include "mim/phase.h"

namespace mim::plug::sflow::phase {

using namespace mim;

class Reduciblifier : public mim::NestPhase<Lam> {
public:
    Reduciblifier(World& world, flags_t annex)
        : NestPhase(world, annex, true) {}

    void visit(const Nest& nest) override { visit(nest.root()); }

private:
    void visit(const Nest::Node* node) {
        for (auto& scc : node->topo()) {
            for (auto child : *scc) {
                // Make deeper loops reducible first.
                visit(child);
            }

            if (scc->size() > 1) {
                // TODO: Check if loop actually has multiple entries
            }
        }
    }
};

} // namespace mim::plug::sflow::phase
