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

    swap(old_world(), new_world());
}

void EggRewrite::convert_lam(MimNode node) {}

// (con <name> <arg_tuple> <body>)
// i.e. (con foo (tuple (var a Nat) (var ret (cn Nat))) (app ret a))
void EggRewrite::convert_con(MimNode node) {
    auto con_name  = get_symbol(node.children[0]);
    auto arg_tuple = get_node(node.children[1]);
    assert(arg_tuple.kind == MimKind::Tuple);

    auto var_names = std::vector<std::string>();
    auto var_types = DefVec{};
    auto ret_type  = DefVec{};
    for (auto child : arg_tuple.children) {
        auto var = get_node(child);
        assert(var.kind == MimKind::Var);

        auto var_name = get_symbol(var.children[0]);
        auto var_type = get_def(var.children[1]);
        var_names.push_back(var_name.c_str());

        if (child != arg_tuple.children.back())
            var_types.push_back(var_type);
        else
            ret_type.push_back(var_type);
    }

    auto new_con = new_world().mut_fun(var_types, ret_type)->set(con_name.c_str());
    new_con->set_body(get_def(node.children[2]));
    add_def(node, new_con);

    add_symbol(con_name, new_con);
    auto i = 0;
    for (auto var : new_con->vars()) {
        auto var_name = var_names[i];
        add_symbol(var_name, var);
    }
}

void EggRewrite::convert_app(MimNode node) {
    // TODO:
    // case 1: the callee is a symbol (i.e. a variable or an axiom)
    // case 2: the callee is a definition (i.e. a lambda or an app)
    auto callee = get_symbol(node.children[0]);
    auto arg    = get_def(node.children[1]);

    if (callee == "%core.nat.add") {
        // more generally, if the symbol refers to an annex
        auto new_call = new_world().call(callee, arg);
        add_def(node, new_call);
    } else {
        // if it refers, to something previously defined and referred to by name, like a variable
        // or a lambda, or anything else
        auto new_app = new_world().app(sym_table_[callee], arg);
        add_def(node, new_app);
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
    auto lit_val = get_num(node.children[0]);
    auto new_lit = new_world().lit_nat(lit_val);
    add_def(node, new_lit);
}

void EggRewrite::convert_tuple(MimNode node) {
    auto ops = DefVec{};
    for (auto child : node.children)
        // TODO: the indices between 'added_' and 'res_'
        // will only match if we add something for each and every
        // MimNode, meaning that we want some kind of dummy nodes
        // in the cases of Num and Symbol, for instance.
        ops.push_back(get_def(child));
    auto new_tuple = new_world().tuple(ops);
    add_def(node, new_tuple);
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
