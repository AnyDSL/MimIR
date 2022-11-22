#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class AnalysisFactory;

using LamSet = GIDSet<Lam*>;

class Utils : public Analysis {
    NomMap<std::unique_ptr<Scope>> scopes;
    DefMap<bool> loop_indices_;
    DefMap<std::unique_ptr<DefSet>> load_deps_;
    LamSet lams_;

    void find_lams(Lam* lam) {
        if (!lams_.insert(lam).second) return;
        for_each_lam(lam->body(), [&](Lam* lam) { find_lams(lam); });
    }

public:
    explicit Utils(AnalysisFactory& factory);

    LamSet& lams() { return lams_; }

    Scope& scope(Lam* lam) {
        auto it = scopes.find(lam);
        if (it == scopes.end()) { it = scopes.emplace(lam, std::make_unique<Scope>(lam)).first; }
        return *it->second;
    }

    bool is_loop_index(const Def* def);

    bool is_root_var(const Def* def);

    DefSet& depends_on_loads(const Def* lam);
};

} // namespace thorin::autodiff
