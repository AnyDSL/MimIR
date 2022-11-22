#include "dialects/autodiff/analysis/analysis.h"

#include "dialects/autodiff/analysis/analysis_factory.h"

namespace thorin::autodiff {

Analysis::Analysis(AnalysisFactory& factory)
    : factory_(factory) {}

AnalysisFactory& Analysis::factory() { return factory_; }

} // namespace thorin::autodiff
