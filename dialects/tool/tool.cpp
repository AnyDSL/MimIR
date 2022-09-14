#include "dialects/tool/tool.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

#include "dialects/tool/passes/set_filter.h"

using namespace thorin;

/// heart of the dialect
/// registers passes in the different optimization phases
/// as well as normalizers for the axioms
extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"tool",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_codegen_prep_phase([](PassMan& man) { man.add<thorin::tool::SetFilter>(); });
            },
            nullptr, [](Normalizers& normalizers) { tool::register_normalizers(normalizers); }};
}
