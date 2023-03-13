#include "dialects/direct/passes/cps2ds.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/direct.h"

namespace thorin::direct {

void CPS2DS::enter() {
    Lam* lam = curr_nom();
    rewrite_lam(lam);
}

void CPS2DS::rewrite_lam(Lam* lam) {
    if (rewritten_lams.contains(lam)) return;
    rewritten_lams.insert(lam);

    if (!lam->isa_nom()) {
        lam->world().DLOG("skipped non-nom {}", lam);
        return;
    }
    if (!lam->is_set()) {
        lam->world().DLOG("skipped non-set {}", lam);
        return;
    }
    if (lam->codom()->isa<Type>()) {
        world().DLOG("skipped type {}", lam);
        return;
    }

    lam->world().DLOG("Rewrite lam: {}", lam->sym());

    lam_stack.push_back(curr_lam_);
    curr_lam_ = lam;

    auto result = rewrite_body(curr_lam_->body());
    // curr_lam_ might be different at this point (newly introduced continuation).
    auto& w = curr_lam_->world();
    w.DLOG("Result of rewrite {} in {}", lam, curr_lam_);
    curr_lam_->set_body(result);

    curr_lam_ = lam_stack.back();
    lam_stack.pop_back();
}

const Def* CPS2DS::rewrite_body(const Def* def) {
    if (!def) return nullptr;
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;
    auto new_def    = rewrite_body_(def);
    rewritten_[def] = new_def;
    return rewritten_[def];
}

const Def* CPS2DS::rewrite_body_(const Def* def) {
    auto& world = def->world();
    if (auto app = def->isa<App>()) {
        auto callee     = app->callee();
        auto args       = app->arg();
        auto new_callee = rewrite_body(callee);
        auto new_arg    = rewrite_body(app->arg());

        if (auto fun_app = new_callee->isa<App>()) {
            if (auto ty_app = fun_app->callee()->isa<App>(); ty_app) {
                if (auto axiom = ty_app->callee()->isa<Axiom>()) {
                    if (axiom->flags() == ((flags_t)Axiom::Base<cps2ds_dep>)) {
                        world.DLOG("rewrite callee {} : {}", callee, callee->type());
                        world.DLOG("rewrite args {} : {}", args, args->type());
                        world.DLOG("rewrite cps axiom {} : {}", ty_app, ty_app->type());
                        // TODO: rewrite function?
                        auto cps_fun = fun_app->arg();
                        cps_fun      = rewrite_body(cps_fun);
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

                        // TODO: use reduce (beta reduction)
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

                        auto new_name = world.append_suffix(curr_lam_->sym(), "_cps_cont");

                        // The continuation that receives the result of the cps function call.
                        auto fun_cont = world.nom_lam(world.cn(inst_ret_ty))->set(new_name);
                        rewritten_lams.insert(fun_cont);
                        // Generate the cps function call `f a` -> `f_cps(a,cont)`
                        auto cps_call = world.app(cps_fun, {new_arg, fun_cont})->set("cps_call");
                        world.DLOG("  curr_lam {}", curr_lam_->sym());
                        curr_lam_->set_body(cps_call);

                        // Fixme: would be great to PE the newly added overhead away..
                        // The current PE just does not terminate on loops.. :/
                        // TODO: Set filter (inline call wrapper)
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

        return world.app(new_callee, new_arg);
    }

    if (auto lam = def->isa_nom<Lam>()) {
        rewrite_lam(lam);
        return lam;
    }

    if (auto tuple = def->isa<Tuple>()) {
        DefArray elements(tuple->ops(), [&](const Def* op) { return rewrite_body(op); });
        return world.tuple(elements)->set(tuple->dbg());
    }

    DefArray new_ops{def->ops(), [&](const Def* op) { return rewrite_body(op); }};

    world.DLOG("def {} : {} [{}]", def, def->type(), def->node_name());

    // TODO: where does this come from?
    // example: ./build/bin/thorin -d matrix -d affine -d direct lit/matrix/read_transpose.thorin -o - -VVVV
    if (def->isa<Infer>()) {
        world.WLOG("infer node {} : {} [{}]", def, def->type(), def->node_name());
        return def;
    }

    return def->rebuild(world, def->type(), new_ops);
}

} // namespace thorin::direct
