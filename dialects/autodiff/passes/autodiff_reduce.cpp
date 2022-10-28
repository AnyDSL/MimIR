#include "dialects/autodiff/passes/autodiff_reduce.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"

namespace thorin::autodiff {

static Lam* duplicate(Lam* lam) {
    auto& w     = lam->world();
    Lam* diffee = w.nom_lam(lam->type()->as<Pi>(), lam->dbg());
    diffee->set(lam->reduce(diffee->var()));
    return diffee;
}

Lam* AutodiffReduce::reduce(Lam* lam) {
    lam         = duplicate(lam);
    auto result = reduce(lam, lam->ret_var());
    return result->as_nom<Lam>();
}

const Def* AutodiffReduce::reduce(const Def* def, const Def* ret) {
    if (def == ret) { return def; }

    auto lam = def->as_nom<Lam>();

    auto& w = world();
    while (true) {
        auto body = lam->body();
        if (auto app = body->isa<App>()) {
            if (match<affine::For>(app)) {
                auto arg = app->arg();
                arg      = arg->refine(4, reduce(app->arg(4)->as_nom<Lam>()));
                arg      = arg->refine(5, reduce(app->arg(5), ret));
                lam->set_body(w.app(app->callee(), arg));
                break;
            } else {
                auto callee = app->callee();
                if (callee == ret) { break; }

                if (callee->is_set()) {
                    auto callee_lam = callee->as_nom<Lam>();
                    auto arg        = app->arg();
                    lam->set(callee_lam->reduce(arg));
                } else {
                    auto last_index = app->num_args() - 1;
                    auto ret_cont   = app->arg(last_index);

                    ret_cont     = reduce(ret_cont, ret);
                    auto new_arg = app->arg()->refine(last_index, ret_cont);

                    lam->set_body(w.app(callee, new_arg));
                    break;
                }
            }
        }
    }

    return lam;
}

const Def* AutodiffReduce::rewrite(const Def* def) {
    if (auto ad_app = match<ad>(def); ad_app && !visited.contains(def)) {
        auto diffee = ad_app->arg()->as_nom<Lam>();
        def         = op_autodiff(reduce(diffee));
        visited.insert(def);
    }

    return def;
}

} // namespace thorin::autodiff
