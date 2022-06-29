#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/direct_style/direct_style.h"
#include "dialects/direct_style/passes/lower_for.h"

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"direct_style",
            [](thorin::PipelineBuilder& builder) {
        builder.extend_opt_phase([](thorin::PassMan& man) { man.add<thorin::direct_style::DS2CPS>(); });
            },
            nullptr, nullptr};
}
