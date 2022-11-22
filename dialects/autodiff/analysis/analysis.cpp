#include "dialects/autodiff/analysis/analysis.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/builder.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

Analysis::Analysis(AnalysisFactory& factory)
    : factory_(factory) {}

AnalysisFactory& Analysis::factory() { return factory_; }

} // namespace thorin::autodiff
