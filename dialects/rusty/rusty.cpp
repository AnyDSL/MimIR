#include "dialects/rusty/rusty.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"rusty",
            [](thorin::PipelineBuilder& builder) {
                // builder.extend_opt_phase([](thorin::PassMan& man) { man.add<thorin::direct::DS2CPS>(); });
            },
            nullptr, [](Normalizers& normalizers) { rusty::register_normalizers(normalizers); }};
}
