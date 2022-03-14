#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include "thorin/error.h"
#include "thorin/world.h"

#include "thorin/be/ll/ll.h"
#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/rw/lower_for.h"

using namespace thorin;

class ForAxiomTest :
    public testing::TestWithParam<std::tuple<int, int, int>> {

};

TEST_P(ForAxiomTest, for) {
    World w;
    auto mem_t  = w.type_mem();
    auto i32_t  = w.type_int_width(32);
    auto i64_t  = w.type_int_width(64);
    auto argv_t = w.type_ptr(w.type_ptr(i32_t));
    const auto [cstart, cstop, cstep] = GetParam();
    auto lit_start = w.lit_int_width(32, cstart);
    auto lit_stop = w.lit_int_width(32, cstop);
    auto lit_step = w.lit_int_width(32, cstep);

    // Cn [mem, i32, ptr(ptr(i32, 0), 0) Cn [mem, i32]]
    auto main_t = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main   = w.nom_lam(main_t, w.dbg("main"));

    {
        auto accumulator_type = w.sigma({i32_t, i64_t});
        auto cont_type        = w.cn({mem_t, accumulator_type});
        auto body_type        = w.cn({mem_t, i32_t, accumulator_type, cont_type});

        auto body = w.nom_lam(body_type, w.dbg("body"));
        {
            auto [mem, i, acctpl, cont] = body->vars<4>({w.dbg("mem"), w.dbg("i"), w.dbg("acctpl"), w.dbg("cont")});
            auto add = w.op(Wrap::add, w.lit_nat(0), w.extract(acctpl, 0_s), i);
            auto mul = w.op(Wrap::mul, w.lit_nat(0), w.extract(acctpl, 1_s), w.op(Conv::u2u, i64_t, i));
            body->app(false, cont, {mem, w.tuple({add, mul})});
        }

        auto brk = w.nom_lam(w.cn({mem_t, accumulator_type}), w.dbg("break"));
        {
            auto [main_mem, argc, argv, ret] =
                main->vars<4>({w.dbg("mem"), w.dbg("argc"), w.dbg("argv"), w.dbg("exit")});
            auto [mem, acctpl] = brk->vars<2>();
            brk->app(false, ret, {mem, w.extract(acctpl, 0_s)});
            main->set_filter(false);
            main->set_body(w.op_for({i32_t, i64_t}, main_mem, lit_start, lit_stop, lit_step, {w.lit_int(0), w.lit_int(i64_t, 5)}, body, brk));
        }
    }

    main->make_external();

    PassMan man{w};
    man.add<LowerFor>();
    man.run();

    PassMan opt{w};
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<CopyProp>(br, ee);
    opt.run();

    std::ofstream file("test.ll");
    Stream s(file);
    ll::emit(w, s);
    file.close();
    
    // TODO make sure that proper clang is in path on Windows
#ifndef _MSC_VER
    unsigned gt = 0;
    for(int i = cstart; i < cstop; i += cstep) {
        gt += i;
    }

    EXPECT_EQ(0, WEXITSTATUS(std::system("clang test.ll -o `pwd`/test")));
    EXPECT_EQ(gt % 255, WEXITSTATUS(std::system("./test")));
#endif
}

// test with these start, stop, step combinations:
INSTANTIATE_TEST_SUITE_P(ForSteps, ForAxiomTest, testing::Combine(testing::Values(0, 2), testing::Values(0, 4, 8), testing::Values(1, 2, 5)));

