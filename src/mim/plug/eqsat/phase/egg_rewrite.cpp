#include "mim/plug/eqsat/phase/egg_rewrite.h"

#include "mim/def.h"
#include "mim/driver.h"

#include "mim/plug/core/autogen.h"

namespace mim::plug::eqsat {

void EggRewrite::start() {
    std::ostringstream sexpr;
    std::cout << "started eqsat phase..\n";

    if (auto sexpr_backend = old_world().driver().backend("sexpr"))
        sexpr_backend(old_world(), sexpr);
    else
        error("EggRewrite: 'sexpr' emitter not loaded; try loading 'core' plugin");

    std::cout << "got the sexpr..\n";

    // NOTE: The symbolic expression 'sexpr' will actually be a series
    // of symbolic expressions, one for each closed mutable Def.
    // Passing 'sexpr' into equality_saturate(sexpr: &str) will return a Vec<RewriteResult>
    // where each RewriteResult represents one rewritten symbolic expression
    // in the form of a recursive expression (a way of representing a symbolic expression as a list of nodes)
    auto rewrites = equality_saturate(sexpr.str());
    std::cout << "got the rewrite results (total: " << rewrites.size() << ")..\n";
    for (auto rewrite : rewrites)
        process(rewrite);

    std::cout << "recreated the world..\n\n";
    swap(old_world(), new_world());
}

void EggRewrite::process(RewriteResult rewrite) {
    res_   = rewrite.value;
    added_ = {};
    vars_  = {};

    for (int id = 0; id < res_.size(); ++id) {
        auto node = set_curr(id);
        std::cout << "init - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << "\n";
        init(node);
    }

    for (int id = 0; id < res_.size(); ++id) {
        auto node = set_curr(id);
        std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << "\n";
        convert(node);
    }
}

void EggRewrite::init(MimNode node) {
    switch (node.kind) {
        case MimKind::Var: init_var(node); break;
        case MimKind::Lam: init_lam(node); break;
        case MimKind::Con: init_con(node); break;
        default: break;
    }
}

void EggRewrite::init_lam(MimNode node) {}

void EggRewrite::init_con(MimNode node) {
    auto con_name = get_symbol(node.children[1]);
    auto domain   = get_node(MimKind::Tuple, node.children[2]);

    DefVec var_types;
    std::vector<std::string> var_names;
    for (auto child : domain.children) {
        auto var      = get_node(MimKind::Var, child);
        auto var_name = get_symbol(var.children[0]);
        auto var_type = get_def(var.children[1]);
        var_names.push_back(var_name);
        var_types.push_back(var_type);
    }

    auto new_con = new_world().mut_con(var_types)->set(con_name.c_str());
    add_var(con_name, new_con);
    add_def(new_con);

    for (int i = 0; auto var : new_con->vars()) {
        auto var_name = var_names[i];
        var->set(var_name);
        add_var(var_name, var);
        i++;
    }
}

void EggRewrite::init_var(MimNode node) {
    // NOTE: Assuming (var <name> <type>)
    // but this might not always be the case.
    // Also, I assume that adding a type to the world again
    // in the convert iteration that we already added here is fine.
    auto var_type = set_curr(node.children[1]);
    convert(var_type, true);
}

void EggRewrite::convert(MimNode node, bool recurse) {
    // NOTE: By putting this loop before the conversion calls
    // we ensure a conversion from the bottom up, which is necessary
    // because some higher level nodes depend on Def's created from their child nodes
    int temp = curr_id_;
    if (recurse) {
        for (int child_id : node.children) {
            auto child_node = set_curr(child_id);
            convert(child_node);
        }
    }
    set_curr(temp);

    switch (node.kind) {
        case MimKind::Lam: convert_lam(node); break;
        case MimKind::Con: convert_con(node); break;
        case MimKind::App: convert_app(node); break;
        case MimKind::Var: convert_var(node); break;
        case MimKind::Lit: convert_lit(node); break;
        case MimKind::Arr: convert_arr(node); break;
        case MimKind::Tuple: convert_tuple(node); break;
        case MimKind::Extract: convert_extract(node); break;
        case MimKind::Ins: convert_ins(node); break;
        case MimKind::Sigma: convert_sigma(node); break;
        case MimKind::Cn: convert_cn(node); break;
        case MimKind::Pi: convert_pi(node); break;
        case MimKind::Idx: convert_idx(node); break;
        case MimKind::Hole: convert_hole(node); break;
        case MimKind::Type: convert_type(node); break;
        case MimKind::Num: convert_num(node); break;
        case MimKind::Symbol: convert_symbol(node); break;
        default: break;
    }
}

void EggRewrite::convert_lam(MimNode node) {}

// (con <extern> <name> <domain> <filter> <body>)
// i.e. (con extern foo (tuple (var a Nat) (var ret (cn nat))) (lit ff) (app ret a))
void EggRewrite::convert_con(MimNode node) {
    auto con       = get_def(curr_id_)->as_mut<Lam>();
    auto is_extern = get_symbol(node.children[0]);
    auto filter    = get_def(node.children[3]);
    auto body      = get_def(node.children[4]);
    con->set_filter(filter);
    con->set_body(body);
    if (is_extern == "extern") con->externalize();
}

// (app <callee> <arg>)
void EggRewrite::convert_app(MimNode node) {
    // TODO:
    // case 1: the callee is a symbol (i.e. a variable or an axiom)
    // case 2: the callee is a definition (i.e. a lambda or an app)
    auto callee = get_symbol(node.children[0]);
    auto arg    = get_def(node.children[1]);

    if (callee == "%core.nat.add") {
        // more generally, if the symbol refers to an annex
        // TODO: there is some bug here when call(callee, arg) calls annex(callee) in world.h (l571 -> l182)
        auto new_call = new_world().call(core::nat::add, arg);
        add_def(new_call);
    } else {
        // if it refers, to something previously defined and referred to by name, like a variable
        // or a lambda, or anything else
        auto var     = get_var(callee);
        auto new_app = new_world().app(var, arg);
        add_def(new_app);
    }
}

// (var <name> <type>)
void EggRewrite::convert_var(MimNode node) {
    auto name = get_symbol(node.children[0]);
    auto var  = get_var(name);
    add_def(var);
}

// (lit <val> [<type>])
void EggRewrite::convert_lit(MimNode node) {
    auto lit_sym = get_symbol(node.children[0]);
    if (lit_sym == "ff") {
        auto new_lit = new_world().lit_ff();
        add_def(new_lit);
        return;
    } else if (lit_sym == "tt") {
        auto new_lit = new_world().lit_tt();
        add_def(new_lit);
        return;
    }

    // TODO: typed literals
    auto lit_val = get_num(node.children[0]);
    auto new_lit = new_world().lit_nat(lit_val);
    add_def(new_lit);
}

void EggRewrite::convert_arr(MimNode node) {}

// (tuple <node> <node> <node> ...)
// TODO: arg tuples of lambda headers shouldn't be added to the world
void EggRewrite::convert_tuple(MimNode node) {
    DefVec ops;
    for (auto child : node.children)
        ops.push_back(get_def(child));
    auto new_tuple = new_world().tuple(ops);
    add_def(new_tuple);
}

void EggRewrite::convert_extract(MimNode node) {}
void EggRewrite::convert_ins(MimNode node) {}

// (sigma (var <name> <type>) (var <name> <type>)) or (sigma <type> <type>)
void EggRewrite::convert_sigma(MimNode node) {
    DefVec ops;
    for (auto child : node.children) {
        auto op = res_[child];
        if (op.kind == MimKind::Var) {
            auto var_type = get_def(op.children[1]);
            ops.push_back(var_type);
        } else {
            auto type = get_def(child);
            ops.push_back(type);
        }
    }

    auto new_sigma = old_world().sigma(ops);
    add_def(new_sigma);
    // so this thing will be referred to in the lambda conversion
    // via var_type = get_def(child_id) and so we need to simply create
    // a sigma via sigma(types). The names of the sigma variables,
    // if there are any, are the projections of the variable which is typed
    // as the sigma accessible via var->proj(idx);
}

void EggRewrite::convert_cn(MimNode node) {
    auto domain = get_def(node.children[0]);
    auto new_cn = new_world().cn(domain);
    add_def(new_cn);
}

void EggRewrite::convert_pi(MimNode node) {}

// (idx <size>)
void EggRewrite::convert_idx(MimNode node) {
    // TODO: what if size is not a number like in (idx 3) but another
    // def as in (idx (app %core.nat.add (lit 1) (lit 2)))
    auto size    = get_num(node.children[0]);
    auto new_idx = new_world().type_idx(size);
    add_def(new_idx);
}

void EggRewrite::convert_hole(MimNode node) {}
void EggRewrite::convert_type(MimNode node) {}

void EggRewrite::convert_num(MimNode node) {}

void EggRewrite::convert_symbol(MimNode node) {
    // TODO: maybe as class attribute instead
    std::unordered_map<std::string, const Def*> sym2type;
    sym2type["top"]  = new_world().type_top();
    sym2type["bot"]  = new_world().type_bot();
    sym2type["bool"] = new_world().type_bool();
    sym2type["nat"]  = new_world().type_nat();
    sym2type["i1"]   = new_world().type_i1();
    sym2type["i2"]   = new_world().type_i2();
    sym2type["i4"]   = new_world().type_i4();
    sym2type["i8"]   = new_world().type_i8();
    sym2type["i16"]  = new_world().type_i16();
    sym2type["i32"]  = new_world().type_i32();
    sym2type["i64"]  = new_world().type_i64();

    auto val = node.symbol.c_str();
    if (sym2type.contains(val)) add_def(sym2type[val]);
}

} // namespace mim::plug::eqsat
