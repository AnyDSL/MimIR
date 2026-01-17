#pragma once

#include "mim/phase.h"

namespace mim {

/// Sparse Conditional Constant Propagation.
/// Lattice per var:
/// ```
///  ⊤      ← Keep as is
///  |
/// Bundle  ← Vars that (horizontally) behave the same build a single congruence class
///  |
/// Expr    ← Whole expressoion is propagated (vertically) through var
///  |
///  ⊥
/// ```
class SCCP : public RWPhase {
private:
    class Analysis : public mim::Analysis {
    public:
        Analysis(World& world)
            : mim::Analysis(world, "SCCP::Analyzer") {}

        auto& lattice() { return lattice_; }

    private:
        const Def* propagate(const Def*, const Def*);
        Def* rewrite_mut(Def*) final;
        const Def* rewrite_imm_App(const App*) final;

        Def2Def lattice_;
    };

public:
    SCCP(World& world, flags_t annex)
        : RWPhase(world, annex, &analysis_)
        , analysis_(world) {}

private:
    const Def* lattice(const Def* def) { return analysis_.lattice()[def]; }
    const Def* rewrite_imm_App(const App*) final;

    Analysis analysis_;
    Lam2Lam lam2lam_;
};

} // namespace mim
