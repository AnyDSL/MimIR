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
    rewrite_lam(lam);
}

/// switches context to new lambda
/// replaces the body
/// note: if the lambda contains a ds call, the body will be replaced
///   with a cps call to the previously ds function and the original
///   body will be altered and stored in a new continuation
void DS2CPS::rewrite_lam(Lam* lam) {
    if (auto i = rewritten_.find(lam); i != rewritten_.end()) return;

    Lam* prev = curr_lam;
    curr_lam  = lam;

    auto ty     = lam->type()->as<Pi>();
    auto arg_ty = ty->dom();
    auto ret_ty = ty->codom();

    world().DLOG("DS2CPS: {} : {} = {} => {}\n", lam->name(), ty, arg_ty, ret_ty);

    // overwrite lam body (or new lambda)
    auto result = rewrite_(curr_lam->body());
    world().DLOG("set body of {} to {} : {}\n", curr_lam->name(), result, result->type());
    curr_lam->set_body(result);

    curr_lam = prev;
    if (!curr_lam) world().debug_stream();
}

/// wrap rewrite calls to avoid code duplication
/// memoize produced rewrite results
/// and return the memoized result if available
const Def* DS2CPS::rewrite_(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if (auto lam = def->isa_nom<Lam>()) {
        // or check below at app
        // or at another point
        // not strictly necessary as enter will reach the lam eventually
        rewrite_lam(lam);
        return lam;
    }

    auto result     = rewrite_inner(def);
    rewritten_[def] = result;
    return result;
}

/// rewrites ds calls to cps calls
/// descents through all other definitions
const Def* DS2CPS::rewrite_inner(const Def* def) {
    World& world = def->world();

    world.DLOG("rewrite {} : {}\n", def->name(), def->type());

    // rewritten_[def] = def;

    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto args   = app->args();

        auto lam = callee->isa_nom<Lam>();

        // manual unfolding instead of match<cps2ds>(callee)
        // due to currying
        Lam* conv_cps       = nullptr;
        auto [axiom, curry] = Axiom::get(callee);
        if (axiom && (axiom->flags() & ~0xFF_u64) == detail::base_value<cps2ds>())
            conv_cps = callee->as<App>()->arg()->as_nom<Lam>();

        if ((lam && !lam->type()->is_cn()) || conv_cps) {
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
            auto arg_ty = ty->as<Pi>()->dom();
            auto ret_ty = ty->as<Pi>()->codom();

            // extend ds function with return continuation
            auto doms = ty->as<Pi>()->doms();
            DefArray cps_dom(doms.size() + 1, [&](size_t i) {
                if (i == doms.size()) {
                    return (const Def*)world.cn(ret_ty);
                } else {
                    return doms[i];
                }
            });

            Lam* lam_cps;
            if (conv_cps) {
                lam_cps = conv_cps;
            } else if (auto i = rewritten_.find(def); i != rewritten_.end()) {
                // already converted lambda available
                lam_cps = (Lam*)i->second;
            } else {
                // "real" ds function

                // cps version of function
                lam_cps = world.nom_lam(world.cn(cps_dom), world.dbg(lam->name() + "_cps"));
                lam_cps->set_filter(lam->filter());

                // each argument is linked to its corresponding argument in the cps function
                for (size_t i = 0; i < lam->num_vars(); ++i) { rewritten_[lam->var((nat_t)i)] = lam_cps->var(i); }
                // reconstruct var -> tuple of var without cont
                // @fun -> (@fun_cps#0,...,@fun_cps#n)
                DefArray real_vars(lam->num_vars(), [&](size_t i) { return lam_cps->var((nat_t)i); });
                rewritten_[lam->var()] = world.tuple(real_vars);
                rewritten_[lam]        = lam_cps;

                // the body of the ds function is the computation result
                lam_cps->set_body(world.app(lam_cps->ret_var(world.dbg("cps_return")), lam->body()));
                // rewrite body directly
                // for correct parameter references (not old ones)
                // ds calls in the function will also be replaced
                // but that would also happen otherwise later on
                rewrite_lam(lam_cps);
            }

            // continuation of call site to receive result
            auto fun_cont = world.nom_lam(world.cn(ret_ty), world.dbg(curr_lam->name() + "_cont"));
            fun_cont->set_filter(curr_lam->filter());

            // f a -> f_cps(a,cont)
            DefArray ext_args(args.size() + 1, [&](size_t i) {
                if (i == args.size()) {
                    return (const Def*)fun_cont;
                } else {
                    return args[i];
                }
            });
            auto cps_call = world.app(lam_cps, ext_args, world.dbg("cps_call"));
            curr_lam->set_body(cps_call);
            curr_lam->set_filter(true);

            world.DLOG("  overwrote body of {} : {} with {} : {}\n", curr_lam, curr_lam->type(), cps_call->name(),
                       cps_call->type());

            // write the body context in the newly created continuation
            // that has access to the result (as its argument)
            curr_lam = fun_cont;
            // result of ds function
            auto [res] = fun_cont->vars<1>();
            // fun_cont->set_body(world.bot());

            world.DLOG("  result {} : {} instead of {} : {}\n", res, res->type(), def, def->type());
            // replace call with the result in the context that will be placed in the continuation
            return res;
        }

        // TODO:
        // are ops rewrites
        // + app calle/arg rewrites
        // all possible combinations?

        // non lam call (handled above)
        auto arg            = app->arg();
        auto args_rewritten = rewrite_(arg);
        auto res            = world.app(rewrite_(callee), args_rewritten);
        return res;
    }

    // TODO: check if lam is necessary or var is enough
    if (auto var = def->isa<Var>()) {
        // do not descend into infinite chain through function
        return def;
    }

    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        auto op   = def->op(i);
        auto nop  = rewrite_(op);
        auto ndef = def->refine(i, nop);
        def       = ndef;
    }

    return def;
}

PassTag* DS2CPS::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::direct
