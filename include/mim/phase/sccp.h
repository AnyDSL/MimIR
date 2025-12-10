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

    private:
        void rewrite_annex(flags_t, const Def*) final;
        void rewrite_external(Def*) final;
        const Def* rewrite_imm_App(const App*) final;
    };

public:
    SCCP(World& world, flags_t annex)
        : RWPhase(world, annex)
        , analysis_(world) {}

private:
    bool analyze() final;
    const Def* init(Def*);
    const Def* init(const Def*);

    std::pair<const Def*, bool> concr2abstr(const Def*, const Def*);
    const Def* concr2abstr(const Def*);
    const Def* concr2abstr_impl(const Def*);
    const Def* join(const Def*, const Def*, const Def*);

    const Def* rewrite_imm_App(const App*) final;

    Analysis analysis_;
    DefSet visited_;
    Def2Def concr2abstr_;
    Lam2Lam lam2lam_;
    bool todo_ = true;
};

} // namespace mim
