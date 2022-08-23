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

const Def* autodiff_type(const Def* arg, const Def* ret) {
    auto& world = arg->world();
    auto aug_arg = augment_type(arg);
    auto aug_ret = augment_type(ret);
    auto tang_arg = tangent_type(arg);
    auto tang_ret = tangent_type(ret);
    // Q* -> P*
    auto pb_ty = world.cn({tang_ret, world.cn({tang_arg})});
    // P' -> Q' * (Q* -> P*)
    auto deriv_ty = 
        world.cn({
            aug_arg,
            world.cn({
                aug_ret,
                pb_ty
            })
        });
    return deriv_ty;
}

// TODO: P->Q => P'->Q' * (Q* -> P*)
const Def* autodiff_type(const Def* ty) {
    auto& world = ty->world();
    // TODO: handle DS (operators)
    if (auto pi = ty->isa<Pi>()) {
        auto [arg, ret_pi] = pi->doms<2>();
        auto ret=ret_pi->as<Pi>()->dom();
        return autodiff_type(arg, ret);
    }
    // TODO: what is this object? (only numbers are printed)
    // possible abstract type from autodiff axiom
    world.ELOG("AutoDiff on type: {}", ty);
    // ty->dump(300);
    // world.write("tmp.txt");
    // can not work with
    // assert(false && "autodiff_type should not be invoked on non-pi types");
    return NULL;
}
} // namespace thorin::autodiff