#include "dialects/debug/debug.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"debug",
            [](thorin::PipelineBuilder& builder) {
                // builder.extend_opt_phase([](thorin::PassMan& man) { man.add<thorin::direct::DS2CPS>(); });
            },
            nullptr, [](Normalizers& normalizers) { debug::register_normalizers(normalizers); }};
}
