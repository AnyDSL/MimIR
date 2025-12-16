#pragma once

#include "mim/phase.h"

namespace mim {

/// Sparse Conditional Constant Propagation.
class SCCP : public RWPhase {
private:
    class Analysis : public mim::Analysis {
    public:
        Analysis(World& world)
            : mim::Analysis(world, "SCCP::Analyzer") {}

        auto& lattice() { return lattice_; }

    private:
        const Def* join(const Def*, const Def*);
        Def* rewrite_mut(Def*) final;
        const Def* rewrite_imm_App(const App*) final;

        Def2Def lattice_;
    };

public:
    SCCP(World& world, flags_t annex)
        : RWPhase(world, annex)
        , analysis_(world) {}

    bool analyze() final;

private:
    auto& lattice() { return analysis_.lattice(); }
    const Def* lattice(const Def* def) { return lattice()[def]; }
    const Def* rewrite_imm_App(const App*) final;

    Analysis analysis_;
    Lam2Lam lam2lam_;
};

} // namespace mim
