#include "dialects/matrix/passes/lower_matrix.h"

#include <iostream>

#include <thorin/lam.h>
#include <thorin/tables.h>

#include "dialects/affine/affine.h"
#include "dialects/matrix/matrix.h"

namespace thorin::matrix {

const Def* LowerMatrix::rewrite(const Def* def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    rewritten[def] = rewrite_(def);
    return rewritten[def];
}

const Def* LowerMatrix::rewrite_(const Def* def) {
    // std::cout << "rewriting " << def << std::endl;

    auto& world = def->world();

    if (auto mapReduce_ax = match<matrix::mapReduce>(def); mapReduce_ax) {
        auto mapReduce_pi = mapReduce_ax->callee_type();

        auto [zero, add, mul, input] =
            mapReduce_ax->args<4>({world.dbg("zero"), world.dbg("add"), world.dbg("mul"), world.dbg("input")});

        world.DLOG("rewriting mapReduce axiom: {}\n", mapReduce_ax);
        world.DLOG("  zero: {}\n", zero);
        world.DLOG("  add: {}\n", add);
        world.DLOG("  mul: {}\n", mul);
        world.DLOG("  input: {}\n", input);

        auto inner_callee = mapReduce_ax->callee()->as<App>();

        auto [n, S, T, m, NI, TI, SI] =
            inner_callee->args<7>({world.dbg("n"), world.dbg("S"), world.dbg("T"), world.dbg("m"), world.dbg("NI"),
                                   world.dbg("TI"), world.dbg("SI")});

        // affine::op_for

        // auto& w = world();
        // w.DLOG("rewriting for axiom: {} within {}", for_ax, curr_nom());

        // auto for_pi  = for_ax->callee_type();
        // auto for_lam = w.nom_lam(for_pi, w.dbg("for"));

        // auto org_body  = for_ax->arg(for_ax->num_args() - 2);
        // auto body_type = org_body->type()->as<Pi>();
        // auto yield_pi  = body_type->doms().back()->as<Pi>();
        // auto yield_lam = w.nom_lam(yield_pi, w.dbg("yield"));

        // { // construct yield
        //     auto [mem, iter, end, step, acc, body, brk] =
        //         for_lam->vars<7>({w.dbg("mem"), w.dbg("begin"), w.dbg("end"), w.dbg("step"), w.dbg("acc"),
        //                           w.dbg("body"), w.dbg("break")});
        //     auto [yield_mem, yield_acc] = yield_lam->vars<2>();

        //     auto add = w.op(Wrap::add, w.lit_nat_0(), iter, step);
        //     yield_lam->app(false, for_lam, {yield_mem, add, end, step, yield_acc, body, brk});
        // }
        // { // construct for
        //     auto [mem, iter, end, step, acc, body, brk] = for_lam->vars<7>();

        //     // continue
        //     auto if_then_cn = w.cn(mem->type());
        //     auto if_then    = w.nom_lam(if_then_cn, nullptr);
        //     if_then->app(false, body, {if_then->var(0, w.dbg("mem")), iter, acc, yield_lam});

        //     // break
        //     auto if_else_cn = w.cn(mem->type());
        //     auto if_else    = w.nom_lam(if_else_cn, nullptr);
        //     if_else->app(false, brk, {if_else->var(0, w.dbg("mem")), acc});

        //     auto cmp = w.op(ICmp::ul, iter, end);
        //     for_lam->branch(false, cmp, if_then, if_else, mem);
        // }

        // return rewritten_[def] = w.app(for_lam, for_ax->arg(), for_ax->dbg());
    }

    return def;
}

PassTag* LowerMatrix::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::matrix
