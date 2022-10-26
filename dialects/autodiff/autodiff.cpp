#include "dialects/autodiff/autodiff.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/affine/passes/lower_for.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/autodiff/passes/autodiff_ext_cleanup.h"
#include "dialects/direct/passes/ds2cps.h"
#include "dialects/mem/passes/rw/reshape.h"

using namespace thorin;

/// optimization idea:
/// * optimize [100]
/// * perform ad [105]
/// * resolve unsolved zeros (not added) [111]
/// * optimize further, cleanup direct style [115-120] (in direct)
/// * cleanup (zeros, externals) [299]
extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"autodiff",
            [](thorin::PipelineBuilder& builder) {
                builder.add_opt(120);
                /*
                                builder.extend_opt_phase(104, [](PassMan& man) {
                                    auto reshape = man.add<mem::Reshape>(mem::Reshape::Arg);
                                    reshape->add_plugin([](mem::Reshape& reshape, const Def* def) {
                                        if (auto app = def->isa<App>()) {
                                            auto callee = app->op(0);

                                            if (auto autodiff = match<autodiff::autodiff>(callee)) {
                                                auto& w     = def->world();
                                                auto arg    = reshape.rewrite(app->op(1));
                                                auto diffee = autodiff->arg();
                                                auto curry  = autodiff->decurry();
                                                if (curry->arg(1) != w.lit_bool(true)) return (const Def*)nullptr;

                                                auto new_diffee   = reshape.rewrite(diffee);
                                                auto new_autodiff = autodiff::op_autodiff(new_diffee,
                   w.lit_bool(false));

                                                arg          = reshape.reshape(arg, new_autodiff->type()->as<Pi>());
                                                auto new_app = w.app(new_autodiff, arg);
                                                return new_app;
                                            }
                                        }

                                        return (const Def*)nullptr;
                                    });
                                });*/
                builder.extend_opt_phase(106, [](thorin::PassMan& man) { man.add<thorin::autodiff::AutoDiffEval>(); });
                builder.extend_opt_phase(107, [](thorin::PassMan& man) { man.add<thorin::affine::LowerFor>(); });
                /*builder.extend_opt_phase(107, [](PassMan& man) {
                    man.add<mem::Reshape>(mem::Reshape::Flat);
                });*/
                builder.extend_opt_phase(126,
                                         [](PassMan& man) { man.add<thorin::autodiff::AutoDiffExternalCleanup>(); });

                builder.add_opt(125);
            },
            nullptr, [](Normalizers& normalizers) { autodiff::register_normalizers(normalizers); }};
}
