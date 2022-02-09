#include "thorin/pass/rw/zip_eval.h"

#include <algorithm>
#include <string>

#include "thorin/analyses/scope.h"

namespace thorin {

#define dlog(world,...) world.DLOG(__VA_ARGS__)
#define type_dump(world,name,d) world.DLOG("{} {} : {}",name,d,d->type())




namespace {

} // namespace

// rewrites applications of the form 'rev_diff function' into the differentiation of f
const Def* ZipEval::rewrite(const Def* def) {
    if(auto lift = isa<Tag::Lift>(def)) {
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
        auto dst =  w.app(w.app(w.app(w.ax_lift(), {r,s}), {n_i,Is,n_o,Os,f}), {a, b});

        auto c_nom  = RWPass<>::curr_nom();
        dlog(w,"Current Nom {}",c_nom);
        auto lam = c_nom->as_nom<Lam>();

        auto cont_lam = w.nom_lam( w.cn_mem(w.type_real()),w.dbg("zip_cont_"+lam->name()) );
        type_dump(w,"created cont:",cont_lam);
        cont_lam->set_filter(true);
        cont_lam->set_body(lam->body());

        lam->set_body(
            w.app(
                cont_lam,
                {
                    lam->mem_var()
                }
                ));


//        auto pb = world_.nom_lam(pb_ty, world_.dbg("pb_alloc"));
//        pb->set_filter(world_.lit_true());
//        auto [z_mem,z] = ZERO(world_,pb->mem_var(),A);
//        pb->set_body( world_.app(pb->ret_var(), {z_mem,z}));
//        THORIN_UNREACHABLE;
        return dst;
    }
//    if (auto app = def->isa<App>()) {
//        if (auto type_app = app->callee()->isa<App>()) {
//            if (auto axiom = type_app->callee()->isa<Axiom>(); axiom && axiom->tag() == Tag::RevDiff) {
    return def;
}

}