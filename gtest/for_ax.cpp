#if 0
#include <fstream>
#include <sstream>

#include <gtest/gtest-param-test.h>

#include "thorin/error.h"
#include "thorin/world.h"

#include "thorin/be/ll/ll.h"
#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/rw/lower_for.h"
#include "thorin/util/sys.h"

#include "helpers.h"

using namespace thorin;

class ForAxiomTest : public testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(ForAxiomTest, for) {
    World w;
    Parser::import_module(w, "affine");

    auto mem_t  = w.type_mem();
    auto i32_t  = w.type_int_width(32);
    auto i64_t  = w.type_int_width(64);
    auto argv_t = w.type_ptr(w.type_ptr(i32_t));

    const auto [cbegin, cend, cstep] = GetParam();

    auto lit_begin = w.lit_int_width(32, cbegin);
    auto lit_end   = w.lit_int_width(32, cend);
    auto lit_step  = w.lit_int_width(32, cstep);

    // Cn [mem, i32, ptr(ptr(i32, 0), 0) Cn [mem, i32]]
    auto main_t = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main   = w.nom_lam(main_t, w.dbg("main"));

    {
        auto accumulator_type = w.sigma({i32_t, i64_t});
        auto yield_type       = w.cn({mem_t, accumulator_type});
        auto body_type        = w.cn({mem_t, i32_t, accumulator_type, yield_type});

        auto body = w.nom_lam(body_type, w.dbg("body"));
        {
            auto [mem, i, acctpl, yield] = body->vars<4>({w.dbg("mem"), w.dbg("i"), w.dbg("acctpl"), w.dbg("yield")});
            auto add                     = w.op(Wrap::add, w.lit_nat(0), w.extract(acctpl, 0_s), i);
            auto mul = w.op(Wrap::mul, w.lit_nat(0), w.extract(acctpl, 1_s), w.op(Conv::u2u, i64_t, i));
            body->app(false, yield, {mem, w.tuple({add, mul})});
        }

        auto brk = w.nom_lam(w.cn({mem_t, accumulator_type}), w.dbg("break"));
        {
            auto [main_mem, argc, argv, ret] =
                main->vars<4>({w.dbg("mem"), w.dbg("argc"), w.dbg("argv"), w.dbg("exit")});
            auto [mem, acctpl] = brk->vars<2>();
            brk->app(false, ret, {mem, w.extract(acctpl, 0_s)});
            main->set_filter(false);
            main->set_body(
                w.op_for(main_mem, lit_begin, lit_end, lit_step, {w.lit_int(0), w.lit_int(i64_t, 5)}, body, brk));
        }
    }

    main->make_external();

    PassMan opt{w};
    opt.add<LowerFor>();
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<CopyProp>(br, ee);
    opt.run();

    unsigned gt = 0;
    for (int i = cbegin; i < cend; i += cstep) { gt += i; }

    EXPECT_EQ(gt % 256, ll::compile_and_run(w, gtest::test_name()));
}

TEST_P(ForAxiomTest, for_dynamic_iters) {
    World w;
    Parser::import_module(w, "affine");

    auto mem_t  = w.type_mem();
    auto i8_t   = w.type_int_width(8);
    auto i32_t  = w.type_int_width(32);
    auto i64_t  = w.type_int_width(64);
    auto argv_t = w.type_ptr(w.arr(w.top_nat(), w.type_ptr(w.arr(w.top_nat(), i8_t))));

    const auto [cbegin, cend, cstep] = GetParam();

    // Cn [mem, i32, ptr(ptr(i32, 0), 0) Cn [mem, i32]]
    auto main_t = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main   = w.nom_lam(main_t, w.dbg("main"));

    auto atoi_ret_t = w.cn({mem_t, i32_t});
    // Cn [:mem, :ptr («⊤∷nat; i8», 0∷nat), Cn [:mem, i32]]
    auto atoi_t     = w.cn({mem_t, w.type_ptr(w.arr(w.top_nat(), i8_t)), atoi_ret_t});
    auto atoi       = w.nom_lam(atoi_t, w.dbg("atoi"));
    auto atoi_begin = w.nom_lam(atoi_ret_t, w.dbg("atoi_cont_begin"));
    auto atoi_end   = w.nom_lam(atoi_ret_t, w.dbg("atoi_cont_end"));
    auto atoi_step  = w.nom_lam(atoi_ret_t, w.dbg("atoi_cont_step"));

    {
        auto [main_mem, argc, argv, ret] = main->vars<4>();

        {
            auto [load_mem, arg_begin] = w.op_load(main_mem, w.op_lea(argv, w.lit_int(i32_t, 1)))->projs<2>();
            main->app(false, atoi, {load_mem, arg_begin, atoi_begin});
        }
        {
            auto [mem, begin]        = atoi_begin->vars<2>();
            auto [load_mem, arg_end] = w.op_load(mem, w.op_lea(argv, w.lit_int(i32_t, 2)))->projs<2>();
            atoi_begin->app(false, atoi, {load_mem, arg_end, atoi_end});
        }
        {
            auto [mem, end]           = atoi_end->vars<2>();
            auto [load_mem, arg_step] = w.op_load(mem, w.op_lea(argv, w.lit_int(i32_t, 3)))->projs<2>();
            atoi_end->app(false, atoi, {load_mem, arg_step, atoi_step});
        }
    }

    {
        auto accumulator_type = w.sigma({i32_t, i64_t});
        auto yield_type       = w.cn({mem_t, accumulator_type});
        auto body_type        = w.cn({mem_t, i32_t, accumulator_type, yield_type});

        auto body = w.nom_lam(body_type, w.dbg("body"));
        {
            auto [mem, i, acctpl, yield] = body->vars<4>({w.dbg("mem"), w.dbg("i"), w.dbg("acctpl"), w.dbg("yield")});
            auto add                     = w.op(Wrap::add, w.lit_nat(0), w.extract(acctpl, 0_s), i);
            auto mul = w.op(Wrap::mul, w.lit_nat(0), w.extract(acctpl, 1_s), w.op(Conv::u2u, i64_t, i));
            body->app(false, yield, {mem, w.tuple({add, mul})});
        }

        auto brk = w.nom_lam(w.cn({mem_t, accumulator_type}), w.dbg("break"));
        {
            auto [main_mem, argc, argv, ret] =
                main->vars<4>({w.dbg("mem"), w.dbg("argc"), w.dbg("argv"), w.dbg("exit")});
            auto [mem, acctpl] = brk->vars<2>();
            brk->app(false, ret, {mem, w.extract(acctpl, 0_s)});

            auto begin            = atoi_begin->var(1, w.dbg("begin"));
            auto end              = atoi_end->var(1, w.dbg("end"));
            auto [step_mem, step] = atoi_step->vars<2>({w.dbg("mem"), w.dbg("step")});
            atoi_step->set_filter(false);
            atoi_step->set_body(w.op_for(step_mem, begin, end, step, {w.lit_int(0), w.lit_int(i64_t, 5)}, body, brk));
        }
    }

    main->make_external();

    PassMan opt{w};
    opt.add<LowerFor>();
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<CopyProp>(br, ee);
    opt.run();

    unsigned gt = 0;
    for (int i = cbegin; i < cend; i += cstep) { gt += i; }

    EXPECT_EQ(gt % 256, ll::compile_and_run(w, gtest::test_name(), fmt("{} {} {}", cbegin, cend, cstep)));
}

// test with these begin, end, step combinations:
INSTANTIATE_TEST_SUITE_P(ForSteps,
                         ForAxiomTest,
                         testing::Combine(testing::Values(0, 2), testing::Values(0, 4, 8), testing::Values(1, 2, 5)));
#endif
