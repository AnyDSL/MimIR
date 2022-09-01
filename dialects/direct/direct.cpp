#include "dialects/direct/direct.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

#include "dialects/direct/passes/ds2cps.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"direct",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase([](thorin::PassMan& man) { man.add<thorin::direct::DS2CPS>(); });
            },
            nullptr, [](Normalizers& normalizers) { direct::register_normalizers(normalizers); }};
}

namespace thorin::direct {} // namespace thorin::direct