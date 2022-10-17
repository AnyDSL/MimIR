#pragma once

#include <queue>

#include "thorin/phase/phase.h"

#include "dialects/clos/clos.h"
#include "dialects/mem/mem.h"

namespace thorin::clos {


class HigherOrderScalerize : public Phase {
public:
    HigherOrderScalerize(World& world)
        : Phase(world, "higher_order_scalerize", true) {}

    void start() override;

private:
    /// Recursively rewrites a Def.
    const Def* rewrite(const Def* def);
    const Def* rewrite_convert(const Def* def);

    const Def* make_scalar(const Def* def);
    //const Def* make_scalar_inv(const Def* def, const Def* ty);

    Def2Def old2new_;
    std::stack<Lam*> worklist_;
};

} // namespace thorin::clos
