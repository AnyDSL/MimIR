#include "dialects/autodiff/autodiff.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/autodiff/passes/autodiff_eval.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"autodiff",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase([](thorin::PassMan& man) { man.add<thorin::autodiff::AutoDiffEval>(); });
            },
            nullptr, [](Normalizers& normalizers) { autodiff::register_normalizers(normalizers); }};
}

namespace thorin::autodiff {
// P => P'
// TODO: function => extend
const Def* augment_type(const Def* ty) { return ty; }
// P => P*
// TODO: nothing? function => R? Mem => R?
const Def* tangent_type(const Def* ty) { return ty; }
// TODO: P->Q => P'->Q' * (Q* -> P*)
const Def* autodiff_type(const Def* ty) {
    auto& world = ty->world();
    world.ELOG("AutoDiff type: {}", ty);
    if (auto pi = ty->isa<Pi>()) {
        auto [arg, ret] = pi->doms<2>();
        world.ELOG("dom 1: {}", arg);
        printf("HI\n");
    }
    return ty;
}
} // namespace thorin::autodiff