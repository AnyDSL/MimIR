#include "dialects/direct/passes/ds2cps.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/direct.h"

namespace thorin::direct {

/// called by the pass manager when the pass is run
/// for every lambda in the code
/// but we need some manual invokation for new lambdas that need urgent care
void DS2CPS::enter() {
    Lam* lam = curr_nom();
    if(!lam->isa_nom()) {
        lam->world().DLOG("skipped non-nom {}", lam);
        return;
    }
    rewrite_lam(lam);
}

/// switches context to new lambda
/// replaces the body
/// note: if the lambda contains a ds call, the body will be replaced
///   with a cps call to the previously ds function and the original
///   body will be altered and stored in a new continuation
void DS2CPS::rewrite_lam(Lam* lam) {
    if (auto i = rewritten_.find(lam); i != rewritten_.end()) return;

    Lam* prev = curr_lam_;
    curr_lam_ = lam;

    auto ty     = lam->type()->as<Pi>();
    auto arg_ty = ty->dom();
    auto ret_ty = ty->codom();

    world().DLOG("DS2CPS: {} : {} = {} => {}", lam->unique_name(), ty, arg_ty, ret_ty);

    if (prev) rewritten_[lam] = lam;

    // overwrite lam body (or new lambda)
    if (auto it = rewritten_bodies_.find(curr_lam_->body()); it != rewritten_bodies_.end())
        curr_lam_->set_body(it->second);
    else {
        auto result = rewrite_(curr_lam_->body());
        world().DLOG("set body of {} to {} : {}", curr_lam_->name(), result, result->type());
        curr_lam_->set_body(result);
    }

    curr_lam_ = prev;
    if (!curr_lam_) world().debug_dump();
}

/// wrap rewrite calls to avoid code duplication
/// memoize produced rewrite results
/// and return the memoized result if available
const Def* DS2CPS::rewrite_(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if (auto lam = def->isa_nom<Lam>()) {
        if (!isa_workable(lam)) return lam;

        auto ty = lam->type();
        if (!ty->is_cn()) {
            // extend ds function with return continuation
            auto doms = ty->as<Pi>()->doms();
            //R DefArray cps_dom(doms.size() + 1, [&](size_t i) -> const Def* {
            //R     if (i == doms.size()) {
            //R         return world().cn(ty->codom());
            //R     } else {
            //R         return doms[i];
            //R     }
            //R });

            // cps version of function
            auto lam_cps = world().nom_lam(world().cn({
                ty->as<Pi>()->dom(),
                world().cn(ty->codom())
            }), world().dbg(lam->name() + "_cps"));
            lam_cps->set_filter(lam->filter());

            // each argument is linked to its corresponding argument in the cps function
            //R for (size_t i = 0; i < lam->num_vars(); ++i) { rewritten_[lam->var((nat_t)i)] = lam_cps->var(i); }
            rewritten_[lam->var()] = lam_cps->var((nat_t)0);
            // reconstruct var -> tuple of var without cont
            // @fun -> (@fun_cps#0,...,@fun_cps#n)
            //R DefArray real_vars(lam->num_vars(), [&](size_t i) { return lam_cps->var((nat_t)i); });
            //R rewritten_[lam->var()] = world().tuple(real_vars);
            rewritten_[lam]        = lam_cps;

            // the body of the ds function is the computation result
            lam_cps->set_body(world().app(lam_cps->ret_var(world().dbg("cps_return")), lam->body()));

            lam = lam_cps;
        }
        rewrite_lam(lam);
        return lam;
    }

    return rewritten_[def] = rewrite_inner(def);
}

/// rewrites ds calls to cps calls
/// descents through all other definitions
const Def* DS2CPS::rewrite_inner(const Def* def) {
    World& world = def->world();

    world.DLOG("rewrite {} : {} aka {}", def->unique_name(), def->type(), def);

    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto args   = app->args();

        // manual unfolding instead of match<cps2ds>(callee)
        // due to currying
        const Lam* conv_cps       = nullptr;
        auto [axiom, curry] = Axiom::get(callee);
        if (axiom && (axiom->flags() & ~0xFF_u64) == detail::base_value<cps2ds>()
            && callee->isa<App>()) {
                // TODO: check if raw app (cps2ds types) and with higher order app
                // (cps2ds types fun) (no args)
            auto callee_function = callee->as<App>()->arg();
            // world.DLOG("callee function {} : {}", callee_function->unique_name(), callee_function->type());
            conv_cps = callee_function->isa<Lam>();
        }

        if ((!axiom && !callee->type()->as<Pi>()->is_cn()) || conv_cps) {
            /*
            h:
              b = f a
              C[b]

            =>

            h:
                f'(a,h_cont)

            h_cont(b):
                C[b]

            f : A -> B
            f': .Cn [A, ret: .Cn[B]]
            */

            auto ty     = callee->type();
            auto ret_ty = ty->as<Pi>()->codom();

            const Def* lam_cps;
            if (conv_cps) {
                lam_cps = conv_cps;
            } else {
                // "real" ds function
                lam_cps = rewrite_(callee);
            }

            // continuation of call site to receive result
            auto fun_cont = world.nom_lam(world.cn(ret_ty), world.dbg(curr_lam_->name() + "_cont"));
            fun_cont->set_filter(curr_lam_->filter());

            // f a -> f_cps(a,cont)
            DefArray ext_args(args.size() + 1, [&](size_t i) {
                if (i == args.size()) {
                    return (const Def*)fun_cont;
                } else {
                    return rewrite_(args[i]);
                }
            });

            auto cps_call = world.app(lam_cps, ext_args, world.dbg("cps_call"));
            if (curr_lam_->body()) rewritten_bodies_[curr_lam_->body()] = cps_call;
            world.DLOG("  overwrite body {} of {} : {} with {} : {}", curr_lam_->body(), curr_lam_, curr_lam_->type(),
                       cps_call->unique_name(), cps_call->type());

            curr_lam_->set_body(cps_call);
            // Fixme: would be great to PE the newly added overhead away..
            // The current PE just does not terminate on loops.. :/
            // curr_lam_->set_filter(true);

            // write the body context in the newly created continuation
            // that has access to the result (as its argument)
            curr_lam_ = fun_cont;
            // result of ds function
            auto res = fun_cont->var();

            world.DLOG("  result {} : {} instead of {} : {}", res, res->type(), def, def->type());
            // replace call with the result in the context that will be placed in the continuation
            return res;
        }

        // TODO:
        // are ops rewrites
        // + app calle/arg rewrites
        // all possible combinations?
    }

    // TODO: check if lam is necessary or var is enough
    if (auto var = def->isa<Var>()) {
        // do not descend into infinite chain through function
        return var;
    }

    if (auto old_nom = def->isa_nom()) { return old_nom; }

    DefArray new_ops{def->ops(), [&](const Def* op) { return rewrite_(op); }};

    if (def->isa<Tuple>()) return world.tuple(new_ops, def->dbg());

    return def->rebuild(world, def->type(), new_ops, def->dbg());
}

} // namespace thorin::direct
