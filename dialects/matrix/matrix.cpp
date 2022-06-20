#include "dialects/matrix.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

// #include "dialects/affine/passes/lower_for.h"

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"matrix",
            [](thorin::PipelineBuilder& builder) {
            },
            nullptr, nullptr};
}
