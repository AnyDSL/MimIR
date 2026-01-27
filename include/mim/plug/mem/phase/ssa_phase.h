#pragma once

#include "mim/phase/sym_expr_opt.h"

namespace mim::plug::mem::phase {

/// Based on: [SSA Translation is an Abstract
/// Interpretation](https://binsec.github.io/assets/publications/papers/2023-popl-full-with-appendices.pdf)
class SSAPhase : public SymExprOpt {
public:
    class Analysis : public mim::SymExprOpt::Analysis {
    public:
        Analysis(World& world)
            : mim::SymExprOpt::Analysis(world, "SSA::Analysis") {}

    private:
        const Def* rewrite_imm_App(const App*) override;
    };

    SSAPhase(World& world, flags_t annex)
        : SymExprOpt(world, annex, &analysis_)
        , analysis_(world) {}

private:
    const Def* rewrite_imm_App(const App*) final;

    Analysis analysis_;
    Lam2Lam lam2lam_;
};

} // namespace mim::plug::mem::phase
