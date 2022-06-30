#include "dialects/direct/passes/ds2cps.h"
#include <thorin/lam.h>
#include "dialects/direct/direct.h"
#include <iostream>

namespace thorin::direct {

void DS2CPS::enter() {
    Lam* lam = curr_nom();
    rewrite_lam(lam);
}

void DS2CPS::rewrite_lam(Lam* lam) {
    if(auto i = rewritten_.find(lam); i != rewritten_.end())
        return;


    Lam* prev = currentLambda;
    currentLambda = lam;

    std::cout << "DS2CPS: " << lam->name() << std::endl;

    // if(lam->name()=="f") {
//         currentLambda->set_body(lam->world().lit_int_width(32,13));
//    } else {
    // overwrite lam body (or new lambda)
    auto result=rewrite_(currentLambda->body());
    std::cout << "set body of " << currentLambda->name() << " to " << result << " : " << result->type() << std::endl;
    currentLambda->set_body(result);
//    }

    // try {
    //     world().debug_stream();
    // } catch(...) {
    // }
    
    currentLambda = prev;
    if(!currentLambda)
        world().debug_stream();
}

const Def* DS2CPS::rewrite_(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;
    auto result = rewrite_inner(def);
    rewritten_[def] = result;
    return result;
}

const Def* DS2CPS::rewrite_inner(const Def* def) {
    World& world = def->world();

    std::cout << "rewrite " << def << " : " << def->type() << std::endl;

    rewritten_[def] = def;

    if(auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto args=app->args();
        if(auto lam = callee->isa_nom<Lam>()) {
            // TODO: check ds 
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
            
            std::cout << "  lam callee " << lam << " : " << lam->type() << std::endl;

            auto ty = lam->type();
            auto arg_ty = ty->as<Pi>()->dom();
            auto ret_ty = ty->as<Pi>()->codom();
            std::cout << "  arg_ty " << arg_ty << std::endl;
            std::cout << "  ret_ty " << ret_ty << std::endl;

            // extend ds function with return continuation
            auto doms = ty->as<Pi>()->doms();
            DefArray cps_dom(doms.size()+1,
                [&](size_t i) {
                    if(i == doms.size()) {
                        return (const Def*)world.cn(ret_ty);
                    } else {
                        return doms[i];
                    }
                }
            );

            // cps version of function
            auto lam_cps = world.nom_lam(world.cn(cps_dom),world.dbg(lam->name()+"_cps"));
            lam_cps->set_filter(lam->filter());
            // continuation of call site to receive result
            auto fun_cont = world.nom_lam(world.cn(ret_ty),world.dbg(currentLambda->name()+"_cont"));
            fun_cont->set_filter(currentLambda->filter());

            // f a -> f_cps(a,cont)
            DefArray ext_args(args.size()+1,
                [&](size_t i) {
                    if(i == args.size()) {
                        return (const Def*)fun_cont;
                    } else {
                        return args[i];
                    }
                }
            );
            auto cps_call=world.app(lam_cps,ext_args,world.dbg("cps_call"));
            currentLambda->set_body(cps_call);
            currentLambda->set_filter(true);


            std::cout << "  overwrote body of " << currentLambda << " : " << currentLambda->type() << " with " << cps_call << " : " << cps_call->type() << std::endl;

            // TODO:
            // DefArray lam_cps_args(args.size(),
            //     [&](size_t i) {
            //         return lam_cps->var(i);
            //     }
            // );

            // rewritten_[lam->var()]=lam_cps->var();
            // auto lam_vars = lam->vars();
            // auto real_vars = lam->vars();
            for (size_t i = 0; i < lam->num_vars(); ++i) {
                rewritten_[lam->var((nat_t)i)]=lam_cps->var(i);
            }
            // reconstruct var -> tuple of var without cont
            DefArray real_vars(lam->num_vars(),
                [&](size_t i) {
                    return lam_cps->var((nat_t)i);
                }
            );
            // rewritten_[lam->var()]=lam_cps->var();
            rewritten_[lam->var()]=world.tuple(real_vars);
            rewritten_[lam]=lam_cps;

            // the body of the ds function is the computation result
            lam_cps->set_body(world.app(
                lam_cps->ret_var(world.dbg("cps_return")),
                lam->body()
                // rewrite_(lam->body())
                // (world.app(lam,lam_cps_args))
            ));

            rewrite_lam(lam_cps);


            // write the body context in the newly created continuation
            // that has access to the result (as its argument)
            currentLambda = fun_cont;
            // result of ds function
            auto [res] = fun_cont->vars<1>();
            // fun_cont->set_body(world.bot());

            std::cout << "  result " << res << " : " << res->type() << " instead of " << def << " : " << def->type() << std::endl;
            // replace call with the result in the context that will be placed in the continuation
            return res;
        }

        // non lam call
        if(auto app = def->isa<App>()){
            auto arg = app->arg();
            auto args_rewritten = rewrite_(arg);
            auto arg_proj = args_rewritten->projs();
            const Def* res;
            res= world.app(rewrite_(callee), arg_proj);
            return res;
        }

    }

    // TODO: check var instead
    if(auto lam = def->isa<Lam>()) {
        // for vars
        return def;
    }


    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        auto op = def->op(i);
        auto nop = rewrite_(op);
        // def->set_op(i, nop);
        std::cout << "  def " << def << " : " << def->type() << " node " << def->node_name() << std::endl;
        std::cout << "  refine " << op << " : " << op->type() << " to (" << nop << ") : " << nop->type() << " [" << nop->node_name() << "]" << std::endl;
        auto ndef=def->refine(i, nop);
        def=ndef;
    }
        // def = def->refine(i, rewrite_(def->op(i)));

    return def;
}

PassTag* DS2CPS::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::direct
