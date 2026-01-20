#pragma once

#include "mim/phase.h"

namespace mim {

/// Symbolic Expression Optimization. Combines:
/// * *[Constant propagation with conditional branches](https://dl.acm.org/doi/pdf/10.1145/103135.103136)* but
/// propagates arbitrary expressions
/// * *[Detecting equality of variables in programs](https://dl.acm.org/doi/10.1145/73560.73561)*
/// * Much in the spirit of *[Combining analyses, combining
/// optimizations](https://dl.acm.org/doi/pdf/10.1145/201059.201061)*.
///
/// Due to MimIR's sea of node structure a number of other optimizations kick in such as arithmetic simplifications and
/// code motion.
///
/// Lattice per Lam::var:
/// ```
///  ⊤      ← Keep as is
///  |
/// Bundle  ← Vars that (horizontally) behave the same build a single congruence class
///  |
/// Expr    ← Whole expression is propagated (vertically) through var
///  |
///  ⊥
/// ```
class SymExprOpt : public RWPhase {
public:
    class Analysis : public mim::Analysis {
    public:
        Analysis(World& world, std::string name = "SEO::Analyzer")
            : mim::Analysis(world, name) {}

        auto& lattice() { return lattice_; }

    protected:
        const Def* propagate(const Def*, const Def*);
        Def* rewrite_mut(Def*) override;
        const Def* rewrite_imm_App(const App*) override;

        Def2Def lattice_;
    };

    SymExprOpt(World& world, flags_t annex, Analysis* analysis = nullptr)
        : RWPhase(world, annex, analysis ? analysis : &analysis_)
        , analysis_(world) {}

protected:
    const Def* rewrite_imm_App(const App*) override;
    const Def* lattice(const Def* def) { return analysis_.lattice()[def]; }

    Analysis analysis_;
    Lam2Lam lam2lam_;
};

} // namespace mim
