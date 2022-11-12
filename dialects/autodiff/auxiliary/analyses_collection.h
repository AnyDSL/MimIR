#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_cache_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_dep_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class AnalysesCollection {
public:
    FlowAnalysis flow_analysis;
    WARAnalysis war_analysis;
    DepAnalysis dep_analysis;
    CacheAnalysis cache_analysis;
    explicit AnalysesCollection(Lam* lam)
        : flow_analysis(lam)
        , war_analysis(lam)
        , dep_analysis(lam)
        , cache_analysis(lam) {}

    FlowAnalysis& flow() { return flow_analysis; }

    WARAnalysis& war() { return war_analysis; }

    DepAnalysis& dep() { return dep_analysis; }

    CacheAnalysis& cache() { return cache_analysis; }
};

} // namespace thorin::autodiff
