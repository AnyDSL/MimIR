#include "dialects/direct/passes/cps2ds.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/direct.h"

namespace thorin::direct {

void CPS2DS::enter() {
    Lam* lam = curr_nom();
    if (!lam->isa_nom()) {
        lam->world().DLOG("skipped non-nom {}", lam);
        return;
    }
    world().DLOG("CPS2DS: {}", lam->name());
    rewrite_lam(lam);
}

void CPS2DS::rewrite_lam(Lam* lam) {
    curr_lam_ = lam;

    auto result = rewrite_body(curr_lam_->body());
    curr_lam_->set_body(result);
}

const Def* CPS2DS::rewrite_body(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;
    rewritten_[def] = rewrite_body_(def);
    return rewritten_[def];
}

const Def* CPS2DS::rewrite_body_(const Def* def) {
    auto& world = def->world();
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto args   = app->arg();
        world.DLOG("rewrite callee {} : {}", callee, callee->type());
        world.DLOG("rewrite args {} : {}", args, args->type());
        auto new_arg = rewrite_body(app->arg());

        if (auto fun_app = callee->isa<App>()) {
            if (auto ty_app = fun_app->callee()->isa<App>(); ty_app) {
                if (auto axiom = ty_app->callee()->isa<Axiom>()) {
                    if (axiom->flags() == ((flags_t)Axiom::Base<cps2ds_dep>)) {
                        world.DLOG("rewrite cps axiom {} : {}", ty_app, ty_app->type());
                        auto cps_fun = fun_app->arg();
                        world.DLOG("function: {} : {}", cps_fun, cps_fun->type());

                        // ```
                        // h:
                        // b = f a
                        // C[b]
                        // ```
                        // =>
                        // ```
                        // h:
                        //     f'(a,h_cont)
                        //
                        // h_cont(b):
                        //     C[b]
                        //
                        // f : A -> B
                        // f': .Cn [A, ret: .Cn[B]]
                        // ```

                        // TODO: rewrite map vs thorin::rewrite
                        // TODO: unify replacements

                        // We instantiate the function type with the applied argument.
                        auto ty     = callee->type();
                        auto ret_ty = ty->as<Pi>()->codom();
                        world.DLOG("callee {} : {}", callee, ty);
                        world.DLOG("new arguments {} : {}", new_arg, new_arg->type());
                        world.DLOG("ret_ty {}", ret_ty);

                        const Def* inst_ret_ty;
                        if (auto ty_pi = ty->isa_nom<Pi>()) {
                            auto ty_dom = ty_pi->var();
                            world.DLOG("replace ty_dom: {} : {} <{};{}>", ty_dom, ty_dom->type(), ty_dom->unique_name(),
                                       ty_dom->node_name());

                            Scope r_scope{ty->as_nom()}; // scope that surrounds ret_ty
                            inst_ret_ty = thorin::rewrite(ret_ty, ty_dom, new_arg, r_scope);
                            world.DLOG("inst_ret_ty {}", inst_ret_ty);
                        } else {
                            inst_ret_ty = ret_ty;
                        }

                        // The continuation that receives the result of the cps function call.
                        auto fun_cont = world.nom_lam(world.cn(inst_ret_ty), world.dbg(curr_lam_->name() + "_cont"));
                        // Generate the cps function call `f a` -> `f_cps(a,cont)`
                        auto cps_call = world.app(cps_fun, {new_arg, fun_cont}, world.dbg("cps_call"));
                        world.DLOG("  curr_lam {}", curr_lam_->name());
                        curr_lam_->set_body(cps_call);

                        // Fixme: would be great to PE the newly added overhead away..
                        // The current PE just does not terminate on loops.. :/
                        // curr_lam_->set_filter(true);

                        // The filter can only be set here (not earlier) as otherwise a debug print causes the "some
                        // operands are set" issue.
                        fun_cont->set_filter(curr_lam_->filter());

                        // We write the body context in the newly created continuation that has access to the result (as
                        // its argument).
                        curr_lam_ = fun_cont;
                        // `res` is the result of the cps function.
                        auto res = fun_cont->var();

                        world.DLOG("  result {} : {} instead of {} : {}", res, res->type(), def, def->type());

                        return res;
                    }
                }
            }
        }

        auto new_calle = rewrite_body(app->callee());
        return world.app(new_calle, new_arg);
    }
    // TODO: are ops rewrites + app calle/arg rewrites all possible combinations?
    // TODO: check if lam is necessary or if var is enough

    // We need this case to not descend into infinite chains through function
    if (auto var = def->isa<Var>()) { return var; }

    if (auto old_nom = def->isa_nom()) { return old_nom; }
    DefArray new_ops{def->ops(), [&](const Def* op) { return rewrite_body(op); }};
    if (def->isa<Tuple>()) return world.tuple(new_ops, def->dbg());

    return def->rebuild(world, def->type(), new_ops, def->dbg());
}

} // namespace thorin::direct
