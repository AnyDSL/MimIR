#include "zip_eval.h"

#include <algorithm>
#include <string>

#include "thorin/analyses/scope.h"

namespace thorin {

#define dlog(world,...) world.DLOG(__VA_ARGS__)
#define type_dump(world,name,d) world.DLOG("{} {} : {}",name,d,d->type())




namespace {

} // namespace

undo_t ZipEval::analyze(const Def* def){
    auto undo = No_Undo;
    if(auto lift = isa<Tag::Zip>(def)) {
        auto& w = def->world();

        dlog(w,"Lift");
        type_dump(w,"Lift",lift);

        auto [a, b] = lift->arg()->projs<2>();
        type_dump(w,"a",a);
        type_dump(w,"b",b);

        auto callee = lift->callee()->as<App>();
        auto is_os = callee->arg();
        dlog(w,"is_os {}",is_os);
        auto [n_i, Is, n_o, Os, f] = is_os->projs<5>();
        auto [r, s] = callee->decurry()->args<2>();
        auto lr = isa_lit(r);
        auto ls = isa_lit(s);

        dlog(w,"r {}",r);
        dlog(w,"s {}",s);

        //        auto dst =  w.app(w.app(w.app(w.ax_lift(), {/*r*/w.lit_nat(2), /*s*/w.tuple({w.lit_nat(2), w.lit_nat(3)})}),
        //                             {/*n_i*/ w.lit_nat(2), /*Is*/w.pack(2, i32_t), /*n_o*/w.lit_nat(1), /*Os*/i32_t, f}),
        //                            {a, b});
        //        auto dst =  w.app(w.app(w.app(w.ax_zip(), {r,s}), {n_i,Is,n_o,Os,f}), {a, b});
        auto dst2 =  w.app(w.app(w.app(w.ax_zip(), {r,s}), {n_i,Is,n_o,Os,f}), {a, b});

        //        auto& w2=world();

        auto c_nom  = curr_nom();
        dlog(w,"Current Nom {}",c_nom);
        auto lam = c_nom->as_nom<Lam>();

        // f'
        auto new_lam = w.nom_lam( lam->type()->as<Pi>(),w.dbg(lam->name()+"_2") );
        new_lam->set_filter(lam->filter());
        // g
        auto cont_lam = w.nom_lam( w.cn(w.type_mem(), w.dbg("")),w.dbg("zip_cont_"+lam->name()) );
        cont_lam->set_filter(false);
        // h
        auto cont_lam2 = w.nom_lam( w.cn_mem(dst2->type()),w.dbg("zip_cont2_"+lam->name()) );
        cont_lam2->set_filter(lam->filter());

        type_dump(w,"created new lam:",new_lam);
        type_dump(w,"created cont:",cont_lam);
        type_dump(w,"created cont2:",cont_lam2);

        new_lam->app( cont_lam, new_lam->mem_var() );
        replace[lam]=new_lam;

        cont_lam->app(cont_lam2,{cont_lam->mem_var(),dst2});
        ignore.emplace(dst2);

        cont_lam2->set_body(lam->body());

        replace[def]=cont_lam2->var(1);
        //        replace[dst]=cont_lam2->var(1);
        //        auto&& [_, ins] = ignore.emplace(dst2);
        //        assert(ins);

        lam->app( cont_lam, lam->mem_var() );

        undo = std::min(undo, undo_visit(lam));
        undo = std::min(undo, undo_visit(cont_lam2));

        //        replacements[lam]={cont_lam,cont_lam2};

        //        return def;
        //        return dst;
        //        return cont_lam2->var(1);


        //        cont_lam->set_body(
        //            lam->body()
        //        );

        //        lam->set_body(
        //            w.app(
        //                cont_lam,
        //                {
        //                    lam->mem_var(),
        //                    dst
        //                }
        //                ));

        //        return cont_lam->var(1);

        //        auto pb = world_.nom_lam(pb_ty, world_.dbg("pb_alloc"));
        //        pb->set_filter(world_.lit_true());
        //        auto [z_mem,z] = ZERO(world_,pb->mem_var(),A);
        //        pb->set_body( world_.app(pb->ret_var(), {z_mem,z}));
        //        THORIN_UNREACHABLE;
        //        return dst;
    }
    return undo;
}

// rewrites applications of the form 'rev_diff function' into the differentiation of f
const Def* ZipEval::rewrite(const Def* def) {
    if(ignore.contains(def)) {
        auto& w = def->world();
        type_dump(w,"ignore ",def);
        return def;
    }
    auto new_def=def;
    auto changed=false;
    if(replace.contains(def)) {
        new_def=replace[def];
        changed=true;
//        return replace[def];
    }
    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        auto opi = def->op(i);
        if (replace.contains(opi)) {
            new_def = new_def->refine(i, replace[opi]);
            changed=true;
        }
    }
    if(changed) {
        auto& w = def->world();
        type_dump(w,"replace ",def);
        type_dump(w,"with ",new_def);
        return new_def;
    }

//    else if(auto lam = def->isa<Lam>()) {
//        type_dump(world()," Lambda",lam);
//        return lam;
//    }

//    if (auto app = def->isa<App>()) {
//        if(auto lam=curr_nom()->isa_nom<Lam>()) {
//            auto& w = def->world();
//            if(app==lam->body()) {
//                type_dump(w,"detected app",app);
//                dlog(w,"Current Nom {}",lam);
//                dlog(w,"Body {}",lam->body());
//                //            if(lam->body())
//                return app;
//            }
//        }
//    }

//        if (auto type_app = app->callee()->isa<App>()) {
//            if (auto axiom = type_app->callee()->isa<Axiom>(); axiom && axiom->tag() == Tag::RevDiff) {
    return def;
}

}