#include <vector>

#include <gtest/gtest-param-test.h>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <mim/pass.h>

#include <mim/ast/parser.h>
#include <mim/pass/beta_red.h>
#include <mim/pass/eta_exp.h>
#include <mim/pass/eta_red.h>
#include <mim/pass/optimize.h>

#include <mim/plug/compile/compile.h>
#include <mim/plug/core/core.h>
#include <mim/plug/math/math.h>
#include <mim/plug/mem/mem.h>

using namespace std::literals;
using namespace mim;
using namespace mim::plug;

// TODO can we port this to lit testing?

TEST(RestrictedDependentTypes, join_singleton) {
    auto test_on_world = [](auto test) {
        Driver driver;
        World& w = driver.world();
        ast::load_plugins(w, {"compile"s, "mem"s, "core"s, "math"s});

        auto i32_t = w.type_i32();
        auto i64_t = w.type_i64();

        auto R = w.axm(w.type())->set("R");
        auto W = w.axm(w.type())->set("W");

        auto RW = w.join({w.uniq(R), w.uniq(W)})->set("RW");
        auto DT = w.join({w.uniq(i32_t), w.uniq(i64_t)})->set("DT");

        auto exp_pi = w.mut_pi(w.type<1>())->set_dom({DT, RW});
        exp_pi->set_codom(w.type());
        auto Exp = w.axm(exp_pi)->set("exp");

        test(w, R, W, Exp);
    };
    {
        std::vector<std::function<void(World&, const Def*, const Def*, const Def*, const Def*, const Def*, const Def*,
                                       const Def*, const Def*)>>
            cases;
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam, {i32_t, R,
                                w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i32_t), w.inj(RW, R)}), w.lit(i32_t, 1000)),
                                w.mut_con(i32_t)}));
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam, {i32_t, W,
                                w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i32_t), w.inj(RW, W)}), w.lit(i32_t, 1000)),
                                w.mut_con(i32_t)}));
        });
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t,
                              auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam, {i64_t, R,
                                w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i64_t), w.inj(RW, R)}), w.lit(i32_t, 1000)),
                                w.mut_con(i64_t)}));
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t,
                              auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam, {i64_t, W,
                                w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i64_t), w.inj(RW, W)}), w.lit(i32_t, 1000)),
                                w.mut_con(i64_t)}));
        });
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float
                        w.app(exp_lam,
                              {w.annex<math::F32>(), R,
                               w.call<core::bitcast>(w.app(Exp, {w.inj(DT, w.annex<math::F32>()), w.inj(RW, R)}),
                                                     w.lit(i32_t, 1000)),
                               w.mut_con(w.annex<math::F32>())}),
                        std::logic_error);
                },
                "std::logic_error");
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float
                        w.app(exp_lam,
                              {w.annex<math::F32>(), W,
                               w.call<core::bitcast>(w.app(Exp, {w.inj(DT, w.annex<math::F32>()), w.inj(RW, W)}),
                                                     w.lit(i32_t, 1000)),
                               w.mut_con(w.annex<math::F32>())}),
                        std::logic_error);
                },
                "std::logic_error");
        });
        cases.emplace_back([](World& w, auto, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // RW fail
                        w.app(exp_lam, {i32_t, i32_t,
                                        w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i32_t), w.inj(RW, i32_t)}),
                                                              w.lit(i32_t, 1000)),
                                        w.mut_con(i32_t)}),
                        std::logic_error);
                },
                "std::logic_error");
        });
        cases.emplace_back([](World& w, auto, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // RW fail
                        w.app(exp_lam, {i64_t, i64_t,
                                        w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i64_t), w.inj(RW, i64_t)}),
                                                              w.lit(i32_t, 1000)),
                                        w.mut_con(i64_t)}),
                        std::logic_error);
                },
                "std::logic_error");
        });

        for (auto&& test : cases) {
            test_on_world([&test](World& w, auto R, auto W, auto Exp) {
                auto i32_t = w.type_i32();
                auto i64_t = w.type_i64();
                auto RW    = w.join({w.uniq(R), w.uniq(W)})->set("RW");
                auto DT    = w.join({w.uniq(i32_t), w.uniq(i64_t)})->set("DT");

                auto exp_sig = w.mut_sigma(4);
                exp_sig->set(0, w.type());
                exp_sig->set(1, w.type());
                exp_sig->set(2, w.app(Exp, {w.inj(DT, exp_sig->var(0_s)), w.inj(RW, exp_sig->var(1_s))}));
                exp_sig->set(3, w.cn(exp_sig->var(0_s)));

                auto exp_lam = w.mut_con(exp_sig);
                exp_lam->app(false, exp_lam->var(3), w.call<core::bitcast>(exp_lam->var(0_s), exp_lam->var(2_s)));
                test(w, R, W, Exp, exp_lam, DT, RW, i32_t, i64_t);
            });
        }
    }
    {
        std::vector<std::function<void(World&, const Def*, const Def*, const Def*, const Def*, const Def*, const Def*,
                                       const Def*, const Def*)>>
            cases;
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i32_t, w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i32_t), w.inj(RW, R)}), w.lit(i32_t, 1000)),
                       w.mut_con(i32_t)}));
        });
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto, auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i64_t, w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i64_t), w.inj(RW, R)}), w.lit(i64_t, 1000)),
                       w.mut_con(i64_t)}));
        });
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float type error
                        w.app(exp_lam,
                              {w.annex<math::F32>(),
                               w.call<core::bitcast>(w.app(Exp, {w.inj(DT, w.annex<math::F32>()), w.inj(RW, R)}),
                                                     w.lit(i32_t, 1000)),
                               w.mut_con(w.annex<math::F32>())}),
                        std::logic_error);
                },
                "std::logic_error");
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_ANY_THROW( // W type error
                w.app(exp_lam,
                      {i32_t, w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i32_t), w.inj(RW, W)}), w.lit(i32_t, 1000)),
                       w.mut_con(i32_t)}));
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t,
                              auto i64_t) {
            EXPECT_ANY_THROW( // W type error
                w.app(exp_lam,
                      {i64_t, w.call<core::bitcast>(w.app(Exp, {w.inj(DT, i64_t), w.inj(RW, W)}), w.lit(i32_t, 1000)),
                       w.mut_con(i64_t)}));
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto, auto) {
            EXPECT_ANY_THROW( // float + W type error (note, the float is not yet what triggers the issue..)
                w.app(exp_lam, {w.annex<math::F32>(),
                                w.call<core::bitcast>(w.app(Exp, {w.inj(DT, w.annex<math::F32>()), w.inj(RW, W)}),
                                                      w.lit(w.annex<math::F32>(), 1000)),
                                w.mut_con(w.annex<math::F32>())}));
        });

        for (auto&& test : cases) {
            test_on_world([&test](World& w, auto R, auto W, auto Exp) {
                auto i32_t = w.type_i32();
                auto i64_t = w.type_i64();
                auto RW    = w.join({w.uniq(R), w.uniq(W)})->set("RW");
                auto DT    = w.join({w.uniq(i32_t), w.uniq(i64_t)})->set("DT");

                auto exp_sig = w.mut_sigma(3);
                exp_sig->set(0, w.type());
                exp_sig->set(1, w.app(Exp, {w.inj(DT, exp_sig->var(0_s)), w.inj(RW, R)}));
                exp_sig->set(2, w.cn(exp_sig->var(0_s)));

                auto exp_lam = w.mut_con(exp_sig);
                exp_lam->app(false, exp_lam->var(2_s), w.call<core::bitcast>(exp_lam->var(0_s), exp_lam->var(1_s)));
                test(w, R, W, Exp, exp_lam, DT, RW, i32_t, i64_t);
            });
        }
    }
}

TEST(RestrictedDependentTypes, ll) {
    Driver driver;
    World& w = driver.world();
    ast::load_plugins(w, {"compile"s, "mem"s, "core"s, "math"s, "opt"s});

    auto mem_t  = w.annex<mem::M>();
    auto i32_t  = w.type_i32();
    auto argv_t = w.call<mem::Ptr0>(w.call<mem::Ptr0>(i32_t));

    // Cn [mem, i32, ptr(ptr(i32, 0), 0) Cn [mem, i32]]
    auto main = w.mut_con({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})})->set("main");
    main->externalize();

    auto R = w.axm(w.type())->set("R");
    auto W = w.axm(w.type())->set("W");

    auto RW = w.join({w.uniq(R), w.uniq(W)})->set("RW");

    auto DT     = w.join({w.uniq(i32_t), w.uniq(w.annex<math::F32>())})->set("DT");
    auto exp_pi = w.mut_pi(w.type<1>())->set_dom({DT, RW});
    exp_pi->set_codom(w.type());

    auto Exp = w.axm(exp_pi)->set("exp");

    auto app_exp = w.app(Exp, {w.inj(DT, i32_t), w.inj(RW, R)});

    {
        auto exp_sig = w.mut_sigma(5);
        exp_sig->set(0, mem_t);
        exp_sig->set(1, w.type());
        exp_sig->set(2, w.type());
        exp_sig->set(3, w.app(Exp, {w.inj(DT, exp_sig->var(1_s)), w.inj(RW, exp_sig->var(2_s))}));
        exp_sig->set(4, w.cn({mem_t, i32_t}));

        auto exp_lam = w.mut_con(exp_sig);
        auto bc      = w.call<core::bitcast>(i32_t, exp_lam->var(3_s));
        exp_lam->app(false, exp_lam->var(4), {exp_lam->var(0_s), bc});

        main->app(false, exp_lam,
                  {main->var(0_s), i32_t, R, w.call<core::bitcast>(app_exp, main->var(1)), main->var(3)});
    }

    optimize(w);
    driver.backend("ll")(w, std::cout);
}
