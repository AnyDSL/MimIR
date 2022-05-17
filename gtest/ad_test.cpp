#include <fstream>

#include <gtest/gtest.h>

#include "thorin/error.h"
#include "thorin/world.h"

#include "thorin/be/ll/ll.h"
#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/rw/auto_diff.h"

//#include "thorin/pass/fp/beta_red.h"
//#include "thorin/pass/fp/copy_prop.h"
//#include "thorin/pass/fp/eta_exp.h"
//#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/auto_diff.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/alloc2malloc.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/remem_elim.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"


using namespace thorin;
TEST(ADTest, square) {
    World w;
    auto mem_t  = w.type_mem();
    auto i32_t  = w.type_int_width(32);
    auto r32_t  = w.type_real(32);
    auto argv_t = w.type_ptr(w.type_ptr(i32_t));

    // Cn [mem, i32, Cn [mem, i32]]
    auto main_t                 = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
//    auto main_t = w.cn({mem_t, w.cn({mem_t, i32_t})});
    auto main = w.nom_lam(main_t, w.dbg("main"));

    auto f_t = w.cn({mem_t, r32_t, w.cn({mem_t, r32_t})});
    auto f = w.nom_lam(f_t, w.dbg("f"));

    auto cont1_t = w.cn({mem_t, r32_t, w.cn({mem_t, r32_t, w.cn({mem_t, r32_t})})});
    auto cont1 = w.nom_lam(cont1_t, w.dbg("cont1"));

    auto cont2_t = w.cn({mem_t});
    auto cont2 = w.nom_lam(cont2_t, w.dbg("cont2"));

    auto cont3_t = w.cn({mem_t, r32_t});
    auto cont3 = w.nom_lam(cont3_t, w.dbg("cont3"));

    auto exit_t = w.cn({mem_t});
    auto exit = w.nom_lam(exit_t, w.dbg("exit"));

    auto success_t = w.cn({mem_t});
    auto success = w.nom_lam(success_t, w.dbg("success"));

//    auto [main_mem, main_ret] = main->vars<2>();
    auto [main_mem, argc, argv, main_ret] = main->vars<4>();

    auto [f_mem, f_arg, f_ret] = f->vars<3>();
    f->app(
        true,
        f_ret,
        {
            f_mem,
            w.op(ROp::mul,(nat_t) 0, f_arg, f_arg)
        }
    );

    auto [cont1_mem, res, pb] = cont1->vars<3>();
    cont1->branch(true,w.op(RCmp::une,(nat_t) 0, res, w.lit_real(32,16.0)), exit, cont2, cont1_mem);

    cont2->app(
        true,
        pb,
        {
            cont2->mem_var(),
            w.lit_real(32,1),
            cont3
        }
   );

    auto [cont3_mem, diff] = cont3->vars<2>();
    cont3->branch(true,w.op(RCmp::une,(nat_t) 0, diff, w.lit_real(32,8.0)), exit, success, cont3_mem);

    exit->app(
        true,
        main_ret,
        {
            exit->mem_var(),
            w.lit_int_width(32,1)
        }
    );

    success->app(
        true,
        main_ret,
        {
            success->mem_var(),
            w.lit_int_width(32,0)
        }
    );

    main->app(
        true,
        w.op_rev_diff(f),
        {
            main_mem,
            w.lit_real(32,4.0),
            cont1
        }
    );

//    printf("DONE");
//    fprintf(stderr,"ERR");

//    Lam* Lam::branch(Filter filter, const Def* cond, const Def* t, const Def* f, const Def* mem, const Def* dbg) {

//
//
//
//
//    auto [mem, ret] = main->vars<2>();
//    main->app(ret, {mem, argc});
    main->make_external();


//    std::ofstream file("test.ll");
//    Stream s(file);
//    ll::emit(w, s);
//    file.close();


    std::ofstream ofs("test_ad.thorin");
    ofs << w;
    ofs.close();

    // replace rev_diff axiom => perform AD
    PassMan::run<AutoDiff>(w);

    std::ofstream ofs2("test_ad_2.thorin");
    ofs2 << w;
    ofs2.close();

    PassMan opt(w);
    opt.add<PartialEval>();
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<SSAConstr>(ee);
    opt.add<Scalerize>(ee);
    opt.add<CopyProp>(br, ee);
    opt.add<TailRecElim>(er);
    opt.run();

    PassMan::run<LamSpec>(w);

    PassMan codgen_prep(w);
    // codgen_prep.add<BoundElim>();
    codgen_prep.add<RememElim>();
    codgen_prep.add<Alloc2Malloc>();
    codgen_prep.add<RetWrap>();
    codgen_prep.run();

    std::ofstream ofs3("test_ad_3.thorin");
    ofs3 << w;
    ofs3.close();

    std::ofstream ofs_ll("test_ad.ll");
    ll::emit(w, ofs_ll);
    ofs_ll.close();

    std::system("clang test_ad.ll -o test_ad");
    assert(0 == WEXITSTATUS(std::system("./test_ad")));
}
