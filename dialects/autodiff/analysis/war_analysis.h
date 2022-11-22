#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/helper.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"
#include "ptr_analysis.h"

namespace thorin::autodiff {

class AnalysisFactory;
class WarAnalysis : public Analysis {
public:
    explicit WarAnalysis(AnalysisFactory& factory);
    void run();

    PtrAnalysis& ptr_analysis;
    DefSet found_;
    DefSet overwritten;

    std::vector<Lam*> lams;

    DefMap<DefSet>* stores;
    DefMap<DefSet> src_stores_;
    DefMap<DefSet> dst_stores_;
    DefSet leas;
    bool todo_ = false;

    bool is_overwritten(const Def* def) { return overwritten.contains(def); }

    bool find(Lam* lam, const Def* mem);

    void find(Lam* lam);

    void meet_stores(const Def* src, const Def* dst);

    bool collect(Lam* lam, const Def* mem);

    void collect(Lam* lam);

    void wipe(Lam* lam);
};

} // namespace thorin::autodiff
