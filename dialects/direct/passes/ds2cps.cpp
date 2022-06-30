#include "dialects/direct/passes/ds2cps.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/direct.h"

#define verbose_rewrite

namespace thorin::direct {

/// called by the pass manager when the pass is run
/// for every lambda in the code
/// but we need some manual invokation for new lambdas that need urgent care
void DS2CPS::enter() {
    Lam* lam = curr_nom();
    rewrite_lam(lam);
}

bool isDS(Lam* lam) {
    auto ty     = lam->type()->as<Pi>();
    auto codom_ty = ty->codom();
    return !(codom_ty->isa<Bot>());

}

/// switches context to new lambda
/// replaces the body
/// note: if the lambda contains a ds call, the body will be replaced
///   with a cps call to the previously ds function and the original
///   body will be altered and stored in a new continuation
void DS2CPS::rewrite_lam(Lam* lam) {
    if (auto i = rewritten_.find(lam); i != rewritten_.end()) return;

    Lam* prev     = currentLambda;
    currentLambda = lam;

    auto ty     = lam->type()->as<Pi>();
    auto arg_ty = ty->dom();
    auto ret_ty = ty->codom();
    std::cout << "DS2CPS: " << lam->name() << " : " << ty << " = " << arg_ty << " => " << ret_ty << std::endl;

    // overwrite lam body (or new lambda)
    auto result = rewrite_(currentLambda->body());
#ifdef verbose_rewrite
    std::cout << "set body of " << currentLambda->name() << " to " << result << " : " << result->type() << std::endl;
#endif
    currentLambda->set_body(result);

    currentLambda = prev;
    if (!currentLambda) world().debug_stream();
}

/// wrap rewrite calls to avoid code duplication
/// memoize produced rewrite results
/// and return the memoized result if available
const Def* DS2CPS::rewrite_(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;
    auto result     = rewrite_inner(def);
    rewritten_[def] = result;
    return result;
}

/// rewrites ds calls to cps calls
/// descents through all other definitions
const Def* DS2CPS::rewrite_inner(const Def* def) {
    World& world = def->world();

#ifdef verbose_rewrite
    std::cout << "rewrite " << def << " : " << def->type() << std::endl;
#endif

    rewritten_[def] = def;

    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto args   = app->args();
        if (auto lam = callee->isa_nom<Lam>(); lam && isDS(lam)) {
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

#ifdef verbose_rewrite
            std::cout << "  lam callee " << lam << " : " << lam->type() << " DS? " << isDS(lam) << std::endl;
#endif

            auto ty     = lam->type();
            auto arg_ty = ty->as<Pi>()->dom();
            auto ret_ty = ty->as<Pi>()->codom();
#if 0
            std::cout << "  arg_ty " << arg_ty << std::endl;
            std::cout << "  ret_ty " << ret_ty << std::endl;
#endif

            // extend ds function with return continuation
            auto doms = ty->as<Pi>()->doms();
            DefArray cps_dom(doms.size() + 1, [&](size_t i) {
                if (i == doms.size()) {
                    return (const Def*)world.cn(ret_ty);
                } else {
                    return doms[i];
                }
            });

            // cps version of function
            auto lam_cps = world.nom_lam(world.cn(cps_dom), world.dbg(lam->name() + "_cps"));
            lam_cps->set_filter(lam->filter());
            // continuation of call site to receive result
            auto fun_cont = world.nom_lam(world.cn(ret_ty), world.dbg(currentLambda->name() + "_cont"));
            fun_cont->set_filter(currentLambda->filter());

            // f a -> f_cps(a,cont)
            DefArray ext_args(args.size() + 1, [&](size_t i) {
                if (i == args.size()) {
                    return (const Def*)fun_cont;
                } else {
                    return args[i];
                }
            });
            auto cps_call = world.app(lam_cps, ext_args, world.dbg("cps_call"));
            currentLambda->set_body(cps_call);
            currentLambda->set_filter(true);

#ifdef verbose_rewrite
            std::cout << "  overwrote body of " << currentLambda << " : " << currentLambda->type() << " with "
                      << cps_call << " : " << cps_call->type() << std::endl;
#endif

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

            // write the body context in the newly created continuation
            // that has access to the result (as its argument)
            currentLambda = fun_cont;
            // result of ds function
            auto [res] = fun_cont->vars<1>();
            // fun_cont->set_body(world.bot());

#ifdef verbose_rewrite
            std::cout << "  result " << res << " : " << res->type() << " instead of " << def << " : " << def->type()
                      << std::endl;
#endif
            // replace call with the result in the context that will be placed in the continuation
            return res;
        }

        // TODO:
        // are ops rewrites
        // + app calle/arg rewrites
        // all possible combinations?

        // non lam call
        if (auto app = def->isa<App>()) {
            auto arg            = app->arg();
            auto args_rewritten = rewrite_(arg);
            auto arg_proj       = args_rewritten->projs();
            const Def* res;
            res = world.app(rewrite_(callee), arg_proj);
            return res;
        }
    }

    // TODO: check if lam is necessary or var is enough
    if (auto var = def->isa<Var>()) {
        // do not descend into infinite chain through function
        return def;
    }

    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        auto op  = def->op(i);
        auto nop = rewrite_(op);
#ifdef verbose_rewrite
        std::cout << "  def " << def << " : " << def->type() << " node " << def->node_name() << std::endl;
        std::cout << "  refine " << op << " : " << op->type() << " to (" << nop << ") : " << nop->type() << " ["
                  << nop->node_name() << "]" << std::endl;
#endif
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
