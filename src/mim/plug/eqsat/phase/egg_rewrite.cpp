#include "mim/plug/eqsat/phase/egg_rewrite.h"

#include "mim/def.h"

#include "mim/plug/core/be/sexpr.h"

// TODO: we have to somehow include the build
// of this header file in cmake as it is now manually
// built from ../egg/ using 'cargo build --release'
// Also, we might want the built files to end up
// in the build directory rather than here in the src directory
#include "../egg/target/cxxbridge/eqsat/src/lib.rs.h"

namespace mim::plug::eqsat {

/*
 * Egg Rewrite
 */
void EggRewrite::start() {
    std::ostringstream sexpr;
    sexpr::emit(old_world(), sexpr);

    auto res_ = equality_saturate(sexpr.str());

    for (auto node : res_)
        if (node.kind == MimKind::Lam)
            convert_lam(node);
        else if (node.kind == MimKind::Con)
            convert_con(node);
        else if (node.kind == MimKind::App)
            convert_app(node);
        else if (node.kind == MimKind::Var)
            convert_var(node);
        else if (node.kind == MimKind::Lit)
            convert_lit(node);
        else if (node.kind == MimKind::Tuple)
            convert_tuple(node);
}

void EggRewrite::convert_lam(MimNode node) {}

void EggRewrite::convert_con(MimNode node) {
    auto sym       = res_[node.children[0]].symbol;
    auto arg_tuple = res_[node.children[1]];
    assert(arg_tuple.kind == MimKind::Tuple);

    auto var_syms = std::vector<std::string>();
    auto domain   = DefVec{};
    auto codomain = DefVec{};
    for (auto child : arg_tuple.children) {
        auto var = res_[child];
        assert(var.kind == MimKind::Var);

        auto var_sym = res_[var.children[0]].symbol;
        var_syms.push_back(var_sym.c_str());
        auto var_type = added_[var.children[1]];

        if (child != arg_tuple.children.back())
            domain.push_back(var_type);
        else
            codomain.push_back(var_type);
    }

    auto new_lam = new_world().mut_fun(domain, codomain)->set(sym.c_str());
    new_lam->set_body(added_[node.children[2]]);
    added_.push_back(new_lam);

    sym_table_[sym] = new_lam;
    auto i          = 0;
    for (auto var : new_lam->vars()) {
        auto var_sym        = var_syms[i];
        sym_table_[var_sym] = var;
    }
}

void EggRewrite::convert_app(MimNode node) {
    auto sym = res_[node.children[0]].symbol;
    auto arg = added_[node.children[1]];

    if (sym == "%core.nat.add") {
        // more generally, if the symbol refers to an annex
        auto new_call = new_world().call(sym, arg);
        added_.push_back(new_call);
    } else {
        // if it refers, to something previously defined, like a variable
        // or a lambda, or anything else
        auto callee  = sym_table_[sym];
        auto new_app = new_world().app(callee, arg);
        added_.push_back(new_app);
    }
}

void EggRewrite::convert_var(MimNode node) {
    // auto sym  = res_[node.children[0]].symbol;
    // auto type = added_[node.children[1]];

    // auto new_var = new_world().var(type);
    // added_.push_back(new_var);
}

void EggRewrite::convert_lit(MimNode node) {
    // This can also be a symbol in the case of "tt" and "ff" literals
    auto lit_val = res_[node.children[0]].num;
    auto new_lit = new_world().lit_nat(lit_val);
    added_.push_back(new_lit);
}

void EggRewrite::convert_tuple(MimNode node) {
    auto ops = DefVec{};
    for (auto child : node.children)
        // TODO: the indices between 'added_' and 'res_'
        // will only match if we add something for each and every
        // MimNode, meaning that we want some kind of dummy nodes
        // in the cases of Num and Symbol, for instance.
        ops.push_back(added_[child]);
    auto new_tuple = new_world().tuple(ops);
    added_.push_back(new_tuple);
}

void EggRewrite::convert_extract(MimNode node) {}
void EggRewrite::convert_ins(MimNode node) {}
void EggRewrite::convert_sigma(MimNode node) {}
void EggRewrite::convert_arr(MimNode node) {}
void EggRewrite::convert_cn(MimNode node) {}
void EggRewrite::convert_idx(MimNode node) {}
void EggRewrite::convert_num(MimNode node) {}
void EggRewrite::convert_symbol(MimNode node) {}

} // namespace mim::plug::eqsat
