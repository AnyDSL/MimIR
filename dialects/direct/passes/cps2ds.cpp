#include "dialects/direct/passes/cps2ds.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/autogen.h"
#include "dialects/direct/direct.h"

namespace thorin::direct {

void CPS2DS::enter() { rewrite_lam(curr_mut()); }

void CPS2DS::rewrite_lam(Lam* lam) {
    if (auto [_, ins] = rewritten_lams.emplace(lam); !ins) return;

    if (lam->isa_imm() || !lam->is_set() || lam->codom()->isa<Type>()) {
        lam->world().DLOG("skipped {}", lam);
        return;
    }

    lam->world().DLOG("Rewrite lam: {}", lam->sym());

    lam_stack.push_back(curr_lam_);
    curr_lam_ = lam;

    auto new_f = rewrite_body(curr_lam_->filter());
    auto new_b = rewrite_body(curr_lam_->body());
    // curr_lam_ might be different at this point (newly introduced continuation).
    world().DLOG("Result of rewrite {} in {}", lam, curr_lam_);
    // TODO This is odd: Why is this *sometimes* not set?
    curr_lam_->unset()->set({new_f, new_b});
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
    if (auto app = def->isa<App>()) {
        auto callee     = app->callee();
        auto arg        = app->arg();
        auto new_callee = rewrite_body(callee);
        auto new_arg    = rewrite_body(arg);

        if (auto cps2ds = match<direct::cps2ds_dep>(new_callee)) {
            world().DLOG("rewrite callee {} : {}", callee, callee->type());
            world().DLOG("rewrite arg  {} : {}", arg, arg->type());
            // TODO: rewrite function?
            auto cps_fun = cps2ds->arg();
            cps_fun      = rewrite_body(cps_fun);
            world().DLOG("function: {} : {}", cps_fun, cps_fun->type());

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
            world().DLOG("callee {} : {}", callee, ty);
            world().DLOG("new arguments {} : {}", new_arg, new_arg->type());
            world().DLOG("ret_ty {}", ret_ty);

            auto inst_ret_ty = ret_ty;
            if (auto pi = ty->isa_mut<Pi>()) inst_ret_ty = pi->reduce(new_arg).back();

            // The continuation that receives the result of the cps function call.
            auto new_name = world().append_suffix(curr_lam_->sym(), "_cps_cont");
            auto fun_cont = world().mut_lam(world().cn(inst_ret_ty))->set(new_name);
            rewritten_lams.insert(fun_cont);

            // Generate the cps function call `f a` -> `f_cps(a,cont)`
            auto cps_call = world().app(cps_fun, {new_arg, fun_cont})->set("cps_call");
            world().DLOG("  curr_lam {}", curr_lam_->sym());
            if (curr_lam_->is_set()) {
                auto filter = curr_lam_->filter();
                curr_lam_->reset({filter, cps_call});
            } else {
                curr_lam_->set(world().lit_ff(), cps_call);
            }

            // Fixme: would be great to PE the newly added overhead away..
            // The current PE just does not terminate on loops.. :/
            // TODO: Set filter (inline call wrapper)
            // curr_lam_->set_filter(true);

            // fun_cont->set_filter(curr_lam_->filter());

            // We write the body context in the newly created continuation that has access to the result
            // (as its argument).
            curr_lam_ = fun_cont;
            // `res` is the result of the cps function.
            auto res = fun_cont->var();

            world().DLOG("  result {} : {} instead of {} : {}", res, res->type(), def, def->type());
            return res;
        }

        return world().app(new_callee, new_arg);
    }

    if (auto lam = def->isa_mut<Lam>()) {
        rewrite_lam(lam);
        return lam;
    }

    if (def->isa<Var>()) return def;

    if (auto tuple = def->isa<Tuple>()) {
        DefArray elements(tuple->ops(), [&](const Def* op) { return rewrite_body(op); });
        return world().tuple(def->type(), elements)->set(tuple->dbg());
    }

    DefArray new_ops{def->ops(), [&](const Def* op) { return rewrite_body(op); }};
    world().DLOG("def {} : {} [{}]", def, def->type(), def->node_name());

    if (def->isa<Infer>()) {
        world().WLOG("infer node {} : {} [{}]", def, def->type(), def->node_name());
        return def;
    }

    if (def->isa<Global>()) return def;

    return def->rebuild(world(), def->type(), new_ops);
}

} // namespace thorin::direct
