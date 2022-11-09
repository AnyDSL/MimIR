#include "dialects/affine/affine.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/affine/passes/lower_for.h"

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"affine",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase([](thorin::PassMan& man) { man.add<thorin::affine::LowerFor>(); });
            },
            nullptr, nullptr};
}
