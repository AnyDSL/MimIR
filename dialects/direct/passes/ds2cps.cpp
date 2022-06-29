#include "dialects/direct/passes/ds2cps.h"
#include <thorin/lam.h>
#include "dialects/direct/direct.h"
#include <iostream>

namespace thorin::direct {

void DS2CPS::enter() {
    Lam* lam = curr_nom();
    // if(auto i = rewritten_.find(lam); i != rewritten_.end())
    //     return;
    Lam* prev = currentLambda;
    currentLambda = lam;

    // overwrite lam body (or new lambda)
    auto result=rewrite_(currentLambda->body());
    currentLambda->set_body(result);
    
    currentLambda = prev;
}

const Def* DS2CPS::rewrite_(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    World& world = def->world();

    std::cout << "rewrite " << def << " : " << def->type() << std::endl;

    rewritten_[def] = def;

    if(auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto args=app->args();
        if(auto lam = callee->isa<Lam>()) {
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

            // TODO: rewrite lambda
            auto ty = lam->type();
            auto arg_ty = ty->as<Pi>()->dom();
            auto ret_ty = ty->as<Pi>()->codom();
            std::cout << "  arg_ty " << arg_ty << std::endl;
            std::cout << "  ret_ty " << ret_ty << std::endl;

            // auto arg = lam->var();
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

            auto lam_cps = world.nom_lam(world.cn(cps_dom),world.dbg(lam->name()+"_cps"));
            lam_cps->set_filter(lam->filter());
            auto fun_cont = world.nom_lam(world.cn(ret_ty),world.dbg(currentLambda->name()+"_cont"));
            fun_cont->set_filter(currentLambda->filter());

            DefArray ext_args(args.size()+1,
                [&](size_t i) {
                    if(i == args.size()) {
                        return (const Def*)fun_cont;
                    } else {
                        return args[i];
                    }
                }
            );

            currentLambda->set_body(world.app(lam_cps,ext_args));
            currentLambda->set_filter(true);
            std::cout << "  overwrote body of " << currentLambda << " : " << currentLambda->type() << std::endl;

            DefArray lam_cps_args(args.size(),
                [&](size_t i) {
                    return lam_cps->var(i);
                }
            );
            lam_cps->set_body(world.app(
                lam_cps->ret_var(),
                (world.app(lam,lam_cps_args))
            ));

            currentLambda = fun_cont;

            auto [res] = fun_cont->vars<1>();

            return rewritten_[def] = res;

            // auto new_callee = rewrite_(lam);
            // auto new_app = world().emit_app(new_callee, app->args());
            // rewritten_[app] = new_app;
            // return new_app;
        }
    }

    for (size_t i = 0, e = def->num_ops(); i != e; ++i) 
        def->refine(i, rewrite_(def->op(i)));

    return def;
}

PassTag* DS2CPS::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::direct
