
#include "mim/phase.h"

namespace mim::plug::sflow::phase {

using namespace mim;

class Structuralizer : public mim::NestPhase<Lam> {
public:
    Structuralizer(World& world, std::string name)
        : NestPhase(world, std::move(name), true) {}

    void visit(const Nest& nest) override {}
};

} // namespace mim::plug::sflow::phase
