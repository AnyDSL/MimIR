#include <fstream>
#include <sstream>

#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include "thorin/def.h"
#include "thorin/error.h"
#include "thorin/tables.h"
#include "thorin/world.h"

#include "thorin/be/ll/ll.h"
#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/rw/lower_for.h"

using namespace thorin;

TEST(RestDepTypes, et) {
    World w;
    auto i32_t = w.type_int_width(32);
    auto i64_t = w.type_int_width(64);

    DefArray type_arr = {i32_t, i64_t};
    auto et_int       = w.et(w.meet(type_arr), {w.lit(i32_t, 66), w.lit(i64_t, 88)});
    et_int->dump(5);

    auto conv = w.pi(et_int, i32_t);
    conv->dump();
    conv->dump(0);
    conv->dump(5);

    auto conv_lam = w.nom_lam(conv, nullptr);
    conv_lam->dump(5);
    auto [var] = conv_lam->vars<1>();
    var->dump();
    var->dump(5);
    auto pick = w.pick(et_int, i32_t);
    pick->dump();
    pick->dump(5);
    conv_lam->set_filter(false);
    conv_lam->set_body(pick);
    conv_lam->dump();
    conv_lam->dump(0);
    conv_lam->dump(5);
}

TEST(RestDepTypes, vel) {
    World w;
    auto i32_t = w.type_int_width(32);
    auto i64_t = w.type_int_width(64);

    auto R = w.axiom(w.type<1>());
    auto W = w.axiom(w.type<1>());

    auto rd = w.lit(R, 0);
    auto wr = w.lit(W, 0);

    auto RW = w.join({R, W});

    auto DT  = w.join({i32_t, w.type_real(32)});
    auto Exp = w.axiom(w.pi({DT, RW}, DT));

    auto ExpApp = w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, rd)});

    // auto Exp = w.nom_pi(w.type())->set_dom({DT, RW});
    // Exp->set_codom(w.tuple({w.vel(DT, Exp->var(0_s)), w.vel(RW, Exp->var(1_s))}));

    // auto fun_pi = w.nom_pi(w.type())->set_dom(ExpApp);
    // fun_pi->set_codom(fun_pi->var(0_s));

    DefArray type_arr = {i32_t, i64_t};
    auto join_t       = w.join(type_arr);
    auto vel_int      = w.vel(join_t, w.lit(i32_t, 5));
    vel_int->dump(5);

    auto conv = w.pi(vel_int, i32_t);
    conv->dump();
    conv->dump(0);
    conv->dump(5);

    auto conv_lam = w.nom_lam(conv, nullptr);
    conv_lam->dump(5);
    auto [var] = conv_lam->vars<1>();
    var->dump();
    var->dump(5);
    auto pick =
        w.test(var, i32_t, w.nom_lam(w.pi(i64_t, i32_t), nullptr), w.nom_lam(w.pi({i64_t, i32_t}, i32_t), nullptr));
    pick->dump();
    pick->dump(5);
    conv_lam->set_filter(false);
    conv_lam->set_body(pick);
    conv_lam->dump();
    conv_lam->dump(0);
    conv_lam->dump(5);
}

TEST(RestDepTypes, arr) {
    return;
    World w;
    auto i32_t = w.type_int_width(32);
    auto i64_t = w.type_int_width(64);

    auto R = w.axiom(w.type());
    auto W = w.axiom(w.type());

    auto RW = w.tuple({R, W});

    auto DT = w.tuple({i32_t, i64_t});

    auto exp_nom_pi = w.nom_pi(w.type())->set_dom({w.type_int(RW->arity()), w.type_int(DT->arity())});
    exp_nom_pi->set_codom(w.tuple({w.extract(RW, exp_nom_pi->var(0_s)), w.extract(DT, exp_nom_pi->var(1))}));
    auto exp = w.axiom(exp_nom_pi);

    auto dom = w.nom_sigma(3);
    dom->set(0, w.type_int(RW->arity()));
    dom->set(1, w.type_int(DT->arity()));
    dom->set(2, w.app(exp, {dom->var(0_s), dom->var(1_s)}));

    auto lam = w.nom_lam(w.cn(dom), nullptr);
    lam->dump(5);
    auto exp_lit = w.lit(w.app(exp, {w.lit_int_mod(2, 1), w.lit_int_mod(2, 0)}), 1);
    auto appd    = w.app(lam, {exp_lit->type()->op(0), exp_lit->type()->op(1), exp_lit});
}

TEST(RestDepTypes, singleton) {
    World w;
    auto i32_t = w.type_int_width(32);
    auto i64_t = w.type_int_width(64);

    auto main = w.nom_lam(w.cn(w.cn(i32_t)), nullptr);
    // main->app(false, main->var(0_s), w.lit_int_width(32, 0));
    main->make_external();

    auto R = w.axiom(w.type(), w.dbg("R"));
    auto W = w.axiom(w.type(), w.dbg("W"));

    auto RW = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));

    auto DT     = w.join({w.singleton(i32_t), w.singleton(w.type_real(32))}, w.dbg("DT"));
    auto exp_pi = w.nom_pi(w.type())->set_dom({DT, RW});
    exp_pi->set_codom(w.type());

    auto Exp = w.axiom(exp_pi, w.dbg("exp"));

    auto app_exp = w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, R)});
    // auto exp_v = w.app(app_exp, w.lit(i32_t, 5));
    auto exp_v = w.lit(app_exp, 10);
    app_exp->dump(0);
    exp_v->dump(0);
    DT->dump(0);

    auto bc_v = w.op_bitcast(exp_v->type()->op(1)->op(0)->as<Vel>()->value(), exp_v);
    bc_v->dump();
    bc_v->dump(0);
    bc_v->dump(5);
    // auto ExpApp = w.app(Exp, w.tuple({w.vel(DT, i32_t), w.vel(RW, R)}));

    {
        auto exp_sig = w.nom_sigma(4);
        exp_sig->set(0, w.type());
        exp_sig->set(1, w.type());
        exp_sig->set(2, w.app(Exp, {w.vel(DT, exp_sig->var(0_s)), w.vel(RW, exp_sig->var(1_s))}));
        exp_sig->set(3, w.cn(exp_sig->var(0_s)));

        auto exp_lam_pi = w.cn(exp_sig);
        auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
        exp_lam->app(false, exp_lam->var(3), w.op_bitcast(exp_lam->var(0_s), exp_lam->var(2_s)));
        exp_lam->dump(0);
        exp_lam->dump(5);

        auto appppp = w.app(exp_lam, {i32_t, R, exp_v, main->var(0_s)});
        appppp->dump(0);
        appppp->dump(5);
    }
    {
        auto exp_sig = w.nom_sigma(3);
        exp_sig->set(0, w.type());
        exp_sig->set(1, w.app(Exp, {w.vel(DT, exp_sig->var(0_s)), w.vel(RW, R)}));
        exp_sig->set(2, w.cn(exp_sig->var(0_s)));

        auto exp_lam_pi = w.cn(exp_sig);
        auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
        exp_lam->app(false, exp_lam->var(2_s), w.op_bitcast(exp_lam->var(0_s), exp_lam->var(1_s)));
        exp_lam->dump(0);
        exp_lam->dump(5);

        auto appppp = w.app(exp_lam, {i32_t, w.op_bitcast(app_exp, w.lit(i32_t, 1000)), main->var(0_s)});
        appppp->dump(0);
        appppp->dump(5);
        // auto apppppW = w.app(exp_lam, {i32_t, w.lit(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, W)}), 112),
        // main->var(0_s)});
    }

    auto app_int = w.app(w.type_int(), w.lit_nat(10));
    auto app_v   = w.lit(app_int, 14);
    app_int->dump(0);
    app_v->dump(0);

    auto sig = w.nom_sigma(3);
    sig->set(0, w.type());
    sig->set(1, w.type());
    sig->set(2, w.sigma({w.vel(DT, sig->var(0_s)), w.vel(RW, sig->var(1))}));
    sig->var(0_s)->dump(0);
    sig->var(1_s)->dump(0);
    auto flexible_pi = w.nom_pi(w.type())->set_dom(sig);
    flexible_pi->set_codom(flexible_pi->var(0_s));

    // sig->dump(0);
    // sig->dump(5);

    auto flexible_lam = w.nom_lam(flexible_pi, nullptr);
    flexible_lam->app(false, main->var(0_s), w.lit_int_width(32, 0_s));
    // flexible_lam->dump();
    // flexible_lam->dump(0);
    // flexible_lam->dump(5);
    auto flexible_app = w.app(flexible_lam, {i32_t, R, w.tuple({w.lit(w.vel(DT, i32_t), 32), w.lit(w.vel(RW, R), 0)})});
    // flexible_app->dump(0);
    auto flexible_app2 =
        w.app(flexible_lam, {i32_t, W, w.tuple({w.lit(w.vel(DT, i32_t), 32), w.lit(w.vel(RW, W), 0)})});

    auto less_flexible_sig = w.nom_sigma(3);
    less_flexible_sig->set(0, w.type());
    less_flexible_sig->set(1, w.sigma({w.vel(DT, less_flexible_sig->var(0_s)), w.vel(RW, R)}));
    less_flexible_sig->set(2, w.cn(less_flexible_sig->var(0_s)));
    auto less_flexible_pi = w.nom_pi(w.type())->set_dom(less_flexible_sig);
    less_flexible_pi->set_codom(less_flexible_pi->var(0_s));

    auto less_flexible_lam = w.nom_lam(less_flexible_pi, nullptr);
    auto less_flexible_app = w.app(
        less_flexible_lam, {i32_t, w.tuple({w.lit(w.vel(DT, i32_t), 1), w.lit(w.vel(RW, R), 0)}), main->var(0_s)});
    EXPECT_THROW(w.app(less_flexible_lam,
                       {i32_t, w.tuple({w.lit(w.vel(DT, i32_t), 1), w.lit(w.vel(RW, W), 0)}), main->var(0_s)}),
                 TypeError);

    less_flexible_sig->dump(0);
    less_flexible_sig->dump(5);

    less_flexible_lam->dump(0);
    less_flexible_lam->dump(5);

    less_flexible_app->dump(0);
    less_flexible_app->dump(5);

    {
        auto [type, exp_val, ret] = less_flexible_lam->vars<3>();
        auto vel                  = w.extract(exp_val, 0_s);
        auto bitcast              = w.op_bitcast(type, vel);
        less_flexible_lam->set_body(w.app(ret, bitcast));
        less_flexible_lam->set_filter(false);
        less_flexible_lam->dump(5);
    }
}

TEST(RestDepTypes, singleton_more_param) {
    World w;
    auto i32_t = w.type_int_width(32);
    auto i64_t = w.type_int_width(64);

    auto main = w.nom_lam(w.cn(w.cn(i32_t)), nullptr);
    main->app(false, main->var(0_s), w.lit_int_width(32, 0));

    auto R = w.axiom(w.type(), w.dbg("R"));
    auto W = w.axiom(w.type(), w.dbg("W"));

    auto RW = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));

    auto DT  = w.join({w.singleton(i32_t), w.singleton(w.type_real(32))}, w.dbg("DT"));
    auto Exp = w.axiom(w.tuple({DT, RW}));

    auto sig = w.nom_sigma(6);
    sig->set(0, w.type());
    sig->set(1, w.type());
    sig->set(2, w.sigma({DT, RW}));
    sig->set(3, sig->var(0_s));
    sig->set(4, sig->var(1_s));
    sig->set(5, w.cn(sig->var(0_s)));
    // sig->var(2_s)->dump(3);

    sig->dump(0);
    sig->dump(5);

    auto flexible_pi = w.nom_pi(w.type())->set_dom(sig);
    flexible_pi->set_codom(flexible_pi->var(0_s));

    auto flexible_lam = w.nom_lam(flexible_pi, nullptr);

    auto flexible_app = w.app(flexible_lam, {i32_t, R, w.tuple({w.vel(DT, i32_t), w.vel(RW, R)}), w.lit(i32_t, 44),
                                             w.lit(R, 0), main->var(0_s)});
    flexible_app->dump(0);

    flexible_lam->app(false, flexible_lam->var(5), flexible_lam->var(3));
}

TEST(RestDepTypes, ll) {
    World w;
    auto mem_t  = w.type_mem();
    auto i32_t  = w.type_int_width(32);
    auto i64_t  = w.type_int_width(64);
    auto argv_t = w.type_ptr(w.type_ptr(i32_t));

    // Cn [mem, i32, ptr(ptr(i32, 0), 0) Cn [mem, i32]]
    auto main_t = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main   = w.nom_lam(main_t, w.dbg("main"));
    main->make_external();

    auto R = w.axiom(w.type(), w.dbg("R"));
    auto W = w.axiom(w.type(), w.dbg("W"));

    auto RW = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));

    auto DT     = w.join({w.singleton(i32_t), w.singleton(w.type_real(32))}, w.dbg("DT"));
    auto exp_pi = w.nom_pi(w.type())->set_dom({DT, RW});
    exp_pi->set_codom(w.type());

    auto Exp = w.axiom(exp_pi, w.dbg("exp"));

    auto app_exp = w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, R)});
    // auto exp_v = w.app(app_exp, w.lit(i32_t, 5));
    auto exp_v = w.lit(app_exp, 10);
    app_exp->dump(0);
    exp_v->dump(0);
    DT->dump(0);

    {
        auto exp_sig = w.nom_sigma(4);
        exp_sig->set(0, mem_t);
        exp_sig->set(1, w.type());
        exp_sig->set(2, w.type());
        exp_sig->set(3, w.app(Exp, {w.vel(DT, exp_sig->var(1_s)), w.vel(RW, exp_sig->var(2_s))}));

        auto exp_lam_pi = w.cn(exp_sig);
        auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
        exp_lam->app(false, main->var(3), {exp_lam->var(0_s), w.op_bitcast(/*exp_lam->var(1_s)*/ i32_t, exp_lam->var(3_s))});
        exp_lam->dump(0);
        exp_lam->dump(5);

        // auto appppp = w.app(exp_lam, {i32_t, R, exp_v, main->var(0_s)});
        // appppp->dump(0);
        // appppp->dump(5);
        main->app(false, exp_lam, {main->var(0_s), i32_t, R, w.op_bitcast(app_exp, main->var(1))});
        main->dump(0);
        main->dump(5);
    }
    std::cout << main;

    w.set_log_level(LogLevel::Debug);
    w.set_log_ostream(&std::cerr);

    PassMan opt{w};
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<CopyProp>(br, ee);
    opt.run();

    ll::emit(w, std::cout);
}
