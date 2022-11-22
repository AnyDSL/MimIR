#include "dialects/autodiff/analysis/affine_cfa.h"

#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/analysis_factory.h"

namespace thorin::autodiff {

AffineCFA::AffineCFA(AnalysisFactory& factory)
    : Analysis(factory) {
    run(factory.lam());
}

} // namespace thorin::autodiff
