#include <fstream>
#include <sstream>

#include <gtest/gtest-param-test.h>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "thorin/def.h"
#include "thorin/dialects.h"
#include "thorin/error.h"
#include "thorin/world.h"

#include "thorin/fe/parser.h"
#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/util/sys.h"

#include "dialects/compile/compile.h"
#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

using namespace thorin;

// TODO can we port this to lit testing?

TEST(RestrictedDependentTypes, join_singleton) {
    auto test_on_world = [](auto test) {
        World w;
        Normalizers normalizers;

        auto compile_d = Dialect::load("compile", {});
        compile_d.register_normalizers(normalizers);
        fe::Parser::import_module(w, "compile", {}, &normalizers);

        auto mem_d = Dialect::load("mem", {});
        mem_d.register_normalizers(normalizers);
        fe::Parser::import_module(w, "mem", {}, &normalizers);

        auto core_d = Dialect::load("core", {});
        core_d.register_normalizers(normalizers);
        fe::Parser::import_module(w, "core", {}, &normalizers);

        auto math_d = Dialect::load("math", {});
        math_d.register_normalizers(normalizers);
        fe::Parser::import_module(w, "math", {}, &normalizers);

        auto i32_t = w.type_int(32);
        auto i64_t = w.type_int(64);

        auto R = w.axiom(w.type(), w.dbg("R"));
        auto W = w.axiom(w.type(), w.dbg("W"));

        auto RW = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));
        auto DT = w.join({w.singleton(i32_t), w.singleton(i64_t)}, w.dbg("DT"));

        auto exp_pi = w.nom_pi(w.type())->set_dom({DT, RW});
        exp_pi->set_codom(w.type());
        auto Exp = w.axiom(exp_pi, w.dbg("exp"));

        test(w, R, W, Exp);
    };
    {
        std::vector<std::function<void(World&, const Def*, const Def*, const Def*, const Def*, const Def*, const Def*,
                                       const Def*, const Def*)>>
            cases;
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i32_t, R, core::op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, R)}), w.lit(i32_t, 1000)),
                       w.nom_lam(w.cn(i32_t), nullptr)}));
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i32_t, W, core::op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                       w.nom_lam(w.cn(i32_t), nullptr)}));
        });
        cases.emplace_back(
            [](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
                EXPECT_NO_THROW( // no type error
                    w.app(exp_lam,
                          {i64_t, R, core::op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, R)}), w.lit(i32_t, 1000)),
                           w.nom_lam(w.cn(i64_t), nullptr)}));
            });
        cases.emplace_back(
            [](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
                EXPECT_NO_THROW( // no type error
                    w.app(exp_lam,
                          {i64_t, W, core::op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                           w.nom_lam(w.cn(i64_t), nullptr)}));
            });
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float
                        w.app(exp_lam, {math::type_f32(w), R,
                                        core::op_bitcast(w.app(Exp, {w.vel(DT, math::type_f32(w)), w.vel(RW, R)}),
                                                         w.lit(i32_t, 1000)),
                                        w.nom_lam(w.cn(math::type_f32(w)), nullptr)}),
                        std::logic_error);
                },
                "std::logic_error");
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float
                        w.app(exp_lam, {math::type_f32(w), W,
                                        core::op_bitcast(w.app(Exp, {w.vel(DT, math::type_f32(w)), w.vel(RW, W)}),
                                                         w.lit(i32_t, 1000)),
                                        w.nom_lam(w.cn(math::type_f32(w)), nullptr)}),
                        std::logic_error);
                },
                "std::logic_error");
        });
        cases.emplace_back([](World& w, auto, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // RW fail
                        w.app(exp_lam,
                              {i32_t, i32_t,
                               core::op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, i32_t)}), w.lit(i32_t, 1000)),
                               w.nom_lam(w.cn(i32_t), nullptr)}),
                        std::logic_error);
                },
                "std::logic_error");
        });
        cases.emplace_back([](World& w, auto, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // RW fail
                        w.app(exp_lam,
                              {i64_t, i64_t,
                               core::op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, i64_t)}), w.lit(i32_t, 1000)),
                               w.nom_lam(w.cn(i64_t), nullptr)}),
                        std::logic_error);
                },
                "std::logic_error");
        });

        for (auto&& test : cases) {
            test_on_world([&test](World& w, auto R, auto W, auto Exp) {
                auto i32_t = w.type_int(32);
                auto i64_t = w.type_int(64);
                auto RW    = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));
                auto DT    = w.join({w.singleton(i32_t), w.singleton(i64_t)}, w.dbg("DT"));

                auto exp_sig = w.nom_sigma(4);
                exp_sig->set(0, w.type());
                exp_sig->set(1, w.type());
                exp_sig->set(2, w.app(Exp, {w.vel(DT, exp_sig->var(0_s)), w.vel(RW, exp_sig->var(1_s))}));
                exp_sig->set(3, w.cn(exp_sig->var(0_s)));

                auto exp_lam_pi = w.cn(exp_sig);
                auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
                exp_lam->app(false, exp_lam->var(3), core::op_bitcast(exp_lam->var(0_s), exp_lam->var(2_s)));
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
                      {i32_t, core::op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, R)}), w.lit(i32_t, 1000)),
                       w.nom_lam(w.cn(i32_t), nullptr)}));
        });
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto, auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i64_t, core::op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, R)}), w.lit(i64_t, 1000)),
                       w.nom_lam(w.cn(i64_t), nullptr)}));
        });
        cases.emplace_back([](World& w, auto R, auto, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float type error
                        w.app(exp_lam, {math::type_f32(w),
                                        core::op_bitcast(w.app(Exp, {w.vel(DT, math::type_f32(w)), w.vel(RW, R)}),
                                                         w.lit(i32_t, 1000)),
                                        w.nom_lam(w.cn(math::type_f32(w)), nullptr)}),
                        std::logic_error);
                },
                "std::logic_error");
        });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto) {
            EXPECT_ANY_THROW( // W type error
                w.app(exp_lam,
                      {i32_t, core::op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                       w.nom_lam(w.cn(i32_t), nullptr)}));
        });
        cases.emplace_back(
            [](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
                EXPECT_ANY_THROW( // W type error
                    w.app(exp_lam,
                          {i64_t, core::op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                           w.nom_lam(w.cn(i64_t), nullptr)}));
            });
        cases.emplace_back([](World& w, auto, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto, auto) {
            EXPECT_ANY_THROW( // float + W type error (note, the float is not yet what triggers the issue..)
                w.app(exp_lam, {math::type_f32(w),
                                core::op_bitcast(w.app(Exp, {w.vel(DT, math::type_f32(w)), w.vel(RW, W)}),
                                                 w.lit(math::type_f32(w), 1000)),
                                w.nom_lam(w.cn(math::type_f32(w)), nullptr)}));
        });

        for (auto&& test : cases) {
            test_on_world([&test](World& w, auto R, auto W, auto Exp) {
                auto i32_t = w.type_int(32);
                auto i64_t = w.type_int(64);
                auto RW    = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));
                auto DT    = w.join({w.singleton(i32_t), w.singleton(i64_t)}, w.dbg("DT"));

                auto exp_sig = w.nom_sigma(3);
                exp_sig->set(0, w.type());
                exp_sig->set(1, w.app(Exp, {w.vel(DT, exp_sig->var(0_s)), w.vel(RW, R)}));
                exp_sig->set(2, w.cn(exp_sig->var(0_s)));

                auto exp_lam_pi = w.cn(exp_sig);
                auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
                exp_lam->app(false, exp_lam->var(2_s), core::op_bitcast(exp_lam->var(0_s), exp_lam->var(1_s)));
                test(w, R, W, Exp, exp_lam, DT, RW, i32_t, i64_t);
            });
        }
    }
}

TEST(RestrictedDependentTypes, ll) {
    World w;
    Normalizers normalizers;
    Passes passes;

    auto compile_d = Dialect::load("compile", {});
    compile_d.register_normalizers(normalizers);
    // compile_d.register_passes(passes);
    fe::Parser::import_module(w, "compile", {}, &normalizers);

    auto mem_d = Dialect::load("mem", {});
    mem_d.register_normalizers(normalizers);
    // mem_d.register_passes(passes);
    fe::Parser::import_module(w, "mem", {}, &normalizers);

    auto core_d = Dialect::load("core", {});
    core_d.register_normalizers(normalizers);
    // core_d.register_passes(passes);
    fe::Parser::import_module(w, "core", {}, &normalizers);

    auto math_d = Dialect::load("math", {});
    math_d.register_normalizers(normalizers);
    // math_d.register_passes(passes);
    fe::Parser::import_module(w, "math", {}, &normalizers);

    auto mem_t  = mem::type_mem(w);
    auto i32_t  = w.type_int(32);
    auto argv_t = mem::type_ptr(mem::type_ptr(i32_t));

    // Cn [mem, i32, ptr(ptr(i32, 0), 0) Cn [mem, i32]]
    auto main_t = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main   = w.nom_lam(main_t, w.dbg("main"));
    main->make_external();

    auto R = w.axiom(w.type(), w.dbg("R"));
    auto W = w.axiom(w.type(), w.dbg("W"));

    auto RW = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));

    auto DT     = w.join({w.singleton(i32_t), w.singleton(math::type_f32(w))}, w.dbg("DT"));
    auto exp_pi = w.nom_pi(w.type())->set_dom({DT, RW});
    exp_pi->set_codom(w.type());

    auto Exp = w.axiom(exp_pi, w.dbg("exp"));

    auto app_exp = w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, R)});

    {
        auto exp_sig = w.nom_sigma(5);
        exp_sig->set(0, mem_t);
        exp_sig->set(1, w.type());
        exp_sig->set(2, w.type());
        exp_sig->set(3, w.app(Exp, {w.vel(DT, exp_sig->var(1_s)), w.vel(RW, exp_sig->var(2_s))}));
        exp_sig->set(4, w.cn({mem_t, i32_t}));

        auto exp_lam_pi = w.cn(exp_sig);
        auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
        auto bc         = core::op_bitcast(i32_t, exp_lam->var(3_s));
        exp_lam->app(false, exp_lam->var(4), {exp_lam->var(0_s), bc});

        main->app(false, exp_lam, {main->var(0_s), i32_t, R, core::op_bitcast(app_exp, main->var(1)), main->var(3)});
    }

    PipelineBuilder builder;
    mem_d.add_passes(builder);
    optimize(w, passes, builder);

    Backends backends;
    core_d.register_backends(backends);
    backends["ll"](w, std::cout);
}
