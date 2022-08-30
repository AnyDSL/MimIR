#include "dialects/direct/direct.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/direct/passes/ds2cps.h"

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"direct",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase([](thorin::PassMan& man) { man.add<thorin::direct::DS2CPS>(); });
            },
            nullptr, nullptr};
}

namespace thorin::direct {

const Def* op_cps2ds(const Def* f) {
    auto& world = f->world();
    // TODO: assert continuation
    auto f_ty = f->type()->as<Pi>();
    auto T    = f_ty->dom(0);
    auto U    = f_ty->dom(1)->as<Pi>()->dom();
    // TODO: check if app can be used instead of raw_app
    return world.raw_app(world.raw_app(world.ax<direct::cps2ds>(), {T, U}), f);
}

} // namespace thorin::direct