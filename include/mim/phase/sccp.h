#pragma once

#include "mim/phase.h"

namespace mim {

/// Based on: [Sparse Conditional Constant Propagation}(https://dl.acm.org/doi/pdf/10.1145/103135.103136)
class SCCP : public RWPhase {
public:
    class Analysis : public mim::Analysis {
    public:
        Analysis(World& world, std::string name = "SCCP::Analysis")
            : mim::Analysis(world, name) {}

        const auto& lattice() { return lattice_; }

    protected:
        Def* rewrite_mut(Def*) override;
        const Def* rewrite_imm_App(const App*) override;

    private:
        const Def* join(const Def*, const Def*);

        Def2Def lattice_;
    };

public:
    SCCP(World& world, flags_t annex, mim::Analysis* analysis = 0)
        : RWPhase(world, annex, analysis ? analysis : &analysis_)
        , analysis_(world) {}

protected:
    const Def* rewrite_imm_App(const App*) override;

    const Def* lattice(const Def* def) {
        if (auto i = analysis_.lattice().find(def); i != analysis_.lattice().end()) return i->second;
        return nullptr;
    }

    Analysis analysis_;
    Lam2Lam lam2lam_;
};

} // namespace mim
