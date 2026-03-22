#include "mim/plug/eqsat/phase/egg_rewrite.h"

#include "mim/def.h"
#include "mim/driver.h"

#include "mim/plug/eqsat/autogen.h"

namespace mim::plug::eqsat {

void EggRewrite::start() {
    auto [rulesets, rules] = import_rules();
    std::cout << "imported rules and rulesets..\n";

    std::ostringstream sexpr;
    if (auto sexpr_backend = driver().backend("sexpr"))
        sexpr_backend(old_world(), sexpr);
    else
        error("EggRewrite: 'sexpr' emitter not loaded; try loading 'core' plugin");

    std::cout << "got the sexpr:\n\n";
    std::cout << pretty(sexpr.str(), 80).c_str() << "\n";

    auto rewrites = equality_saturate(sexpr.str(), rulesets);
    std::cout << "got the rewrite results (total: " << rewrites.size() << ")..\n";
    for (auto rewrite : rewrites)
        process(rewrite);

    swap(old_world(), new_world());
}

std::pair<rust::Vec<RuleSet>, rust::Vec<int>> EggRewrite::import_rules() {
    // Internalize eqsat ruleset config functions (lam with signature [] -> %eqsat.Ruleset)
    DefVec lams;
    for (auto def : old_world().externals().mutate()) {
        if (auto lam = def->isa<Lam>(); lam && lam->num_doms() == 0) {
            if (lam->codom()->sym().view() == "%eqsat.Ruleset") {
                lams.push_back(lam);
                def->internalize();
            }
        } else if (auto rule = def->isa<mim::Rule>()) {
            // TODO: rules are internal how do i access them here?
            std::cout << "Rule found: \n";
            std::cout << rule << "\n";
            std::cout << rule->type() << "\n";
        }
    }

    // Import predefined rulesets and custom rules from config functions
    rust::Vec<RuleSet> rulesets;
    // TODO: int is just a placeholder and should be replaced
    // with the actual type for rules later on
    rust::Vec<int> rules;
    for (auto lam : lams) {
        auto body = lam->as<Lam>()->body();
        if (auto rs = Axm::isa<eqsat::rulesets>(body)) {
            for (auto ruleset : rs->args())
                if (auto core = Axm::isa<eqsat::core>(ruleset))
                    rulesets.push_back(RuleSet::Core);
                else if (auto math = Axm::isa<eqsat::math>(ruleset))
                    rulesets.push_back(RuleSet::Math);

        } else if (auto rs = Axm::isa<eqsat::rules>(body)) {
            // TODO: translate custom rules from %eqsat.rules and pass as another arg
            // to equality_saturate (rules: Vec<Rule> or something)
            // - each custom rule might need to be translated into two
            //   sexpr's for lhs and rhs before passing them on
        }
    }

    return {rulesets, rules};
}

void EggRewrite::process(RewriteResult rewrite) {
    res_   = rewrite.value;
    added_ = {};
    vars_  = {};

    for (int id = 0; id < res_.size(); ++id) {
        auto node = set_curr(id);
        init(node);
    }

    for (int id = 0; id < res_.size(); ++id) {
        auto node = set_curr(id);
        convert(node);
    }
}

void EggRewrite::init(MimNode node) {
    switch (node.kind) {
        case MimKind::Let: init_let(node); break;
        case MimKind::Lam: init_lam(node); break;
        case MimKind::Con: init_con(node); break;
        case MimKind::Var: init_var(node); break;
        default: break;
    }
}

void EggRewrite::init_let(MimNode node) {}

void EggRewrite::init_lam(MimNode node) {}

void EggRewrite::init_con(MimNode node) {
    std::cout << "init - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
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
        std::cout << var << " : " << var->type();
        auto projs = var->projs();
        // TODO: set projections of variables (i.e. sigma vars)
        // for (auto proj : projs) {
        // std::cout << proj << ", ";
        // proj->set("foo");
        // }
        std::cout << " - ";
        i++;
    }
    std::cout << new_con << "\n";
}

void EggRewrite::init_var(MimNode node) {
    // NOTE: Assuming (var <name> <type>)
    // but this might not always be the case. (Anonymous vars)
    std::cout << "init - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << "\n";
    auto var_type = set_curr(node.children[1]);
    convert(var_type, true);
}

void EggRewrite::convert(MimNode node, bool recurse) {
    // NOTE: By putting this loop before the conversion calls
    // we ensure a conversion from the bottom up, which is necessary
    // because some higher level nodes depend on Def's created from their child nodes.
    int temp = curr_id_;
    if (recurse) {
        for (int child_id : node.children) {
            auto child_node = set_curr(child_id);
            convert(child_node, recurse);
        }
    }
    set_curr(temp);

    switch (node.kind) {
        case MimKind::Let: convert_let(node); break;
        case MimKind::Lam: convert_lam(node); break;
        case MimKind::Con: convert_con(node); break;
        case MimKind::App: convert_app(node); break;
        case MimKind::Var: convert_var(node); break;
        case MimKind::Lit: convert_lit(node); break;
        case MimKind::Pack: convert_pack(node); break;
        case MimKind::Tuple: convert_tuple(node); break;
        case MimKind::Extract: convert_extract(node); break;
        case MimKind::Ins: convert_ins(node); break;
        case MimKind::Arr: convert_arr(node); break;
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

// (let <name> <definition> <expression>)
void EggRewrite::convert_let(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto expr = get_def(node.children[2]);
    add_def(expr);
    std::cout << expr << "\n";
}

void EggRewrite::convert_lam(MimNode node) {}

// (con <extern> <name> <domain> <filter> <body>)
// i.e. (con extern foo (tuple (var a Nat) (var ret (cn nat))) (lit ff) (app ret a))
void EggRewrite::convert_con(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto con       = get_def(curr_id_)->as_mut<Lam>();
    auto is_extern = get_symbol(node.children[0]);
    auto filter    = get_def(node.children[3]);
    auto body      = get_def(node.children[4]);
    con->set_filter(filter);
    con->set_body(body);
    if (is_extern == "extern") con->externalize();

    std::cout << con << "\n";
}

// (app <callee> <arg>)
void EggRewrite::convert_app(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto callee  = get_def(node.children[0]);
    auto arg     = get_def(node.children[1]);
    auto new_app = new_world().app(callee, arg);
    add_def(new_app);
    std::cout << new_app << "\n";
}

// (var <name> <type>)
void EggRewrite::convert_var(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto name = get_symbol(node.children[0]);
    auto var  = get_var(name);
    add_def(var);
    std::cout << var << "\n";
}

// (lit <val> [<type>])
void EggRewrite::convert_lit(MimNode node) {
    // Case 1: (lit tt) or (lit ff)
    auto lit_sym = get_symbol(node.children[0]);
    if (lit_sym == "ff") {
        std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
        auto new_lit = new_world().lit_ff();
        add_def(new_lit);
        std::cout << new_lit << "\n";
        return;
    } else if (lit_sym == "tt") {
        std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
        auto new_lit = new_world().lit_tt();
        add_def(new_lit);
        std::cout << new_lit << "\n";
        return;
    }

    auto lit_val = get_num(node.children[0]);
    if (node.children.size() > 1) {
        // Case 2: (lit <val> <type>)
        std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
        auto lit_type = get_def(node.children[1]);
        auto new_lit  = new_world().lit(lit_type, lit_val);
        add_def(new_lit);
        std::cout << new_lit << "\n";
    } else {
        // Case 3: (lit <val>)
        std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
        auto new_lit = new_world().lit_nat(lit_val);
        add_def(new_lit);
        std::cout << new_lit << "\n";
    }
}

// (pack <arity> <body>)
void EggRewrite::convert_pack(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto arity = get_def(node.children[0]);
    auto body  = get_def(node.children[1]);

    auto new_pack = new_world().pack(arity, body);
    add_def(new_pack);
    std::cout << new_pack << "\n";
}

// (tuple <node1> <node2> <node3> ...)
// TODO: arg tuples of lambda headers shouldn't be added to the world
void EggRewrite::convert_tuple(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    DefVec ops;
    for (auto child : node.children) {
        auto op = get_def(child);
        ops.push_back(op);
    }
    auto new_tuple = new_world().tuple(ops);
    add_def(new_tuple);
    std::cout << new_tuple << "\n";
}

// (extract <tuple> <index>)
void EggRewrite::convert_extract(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto tuple = get_def(node.children[0]);
    auto index = get_def(node.children[1]);

    auto new_extract = new_world().extract(tuple, index);
    add_def(new_extract);
    std::cout << new_extract << "\n";
}

// (ins <tuple> <index> <value>)
void EggRewrite::convert_ins(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto tuple = get_def(node.children[0]);
    auto index = get_def(node.children[1]);
    auto value = get_def(node.children[2]);

    auto new_insert = new_world().insert(tuple, index, value);
    add_def(new_insert);
    std::cout << new_insert << "\n";
}

// (arr <arity> <body>)
void EggRewrite::convert_arr(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto arity = get_def(node.children[0]);
    auto body  = get_def(node.children[1]);

    auto new_arr = new_world().arr(arity, body);
    add_def(new_arr);
    std::cout << new_arr << "\n";
}

// (sigma (var <name1> <type1>) (var <name2> <type2>) ...)
//  or
// (sigma <type1> <type2> ...)
void EggRewrite::convert_sigma(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
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

    auto new_sigma = new_world().sigma(ops);
    add_def(new_sigma);
    std::cout << new_sigma << "\n";
}

// (cn <domain>)
void EggRewrite::convert_cn(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto domain = get_def(node.children[0]);
    auto new_cn = new_world().cn(domain);
    add_def(new_cn);
    std::cout << new_cn << "\n";
}

void EggRewrite::convert_pi(MimNode node) {}

// (idx <size>)
void EggRewrite::convert_idx(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto size    = get_def(node.children[0]);
    auto new_idx = new_world().type_idx(size);
    add_def(new_idx);
    std::cout << new_idx << "\n";
}

void EggRewrite::convert_hole(MimNode node) {}
void EggRewrite::convert_type(MimNode node) {}

void EggRewrite::convert_num(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << "\n";
}

void EggRewrite::convert_symbol(MimNode node) {
    std::cout << "convert - current node(" << curr_id_ << "): " << mim_node_str(node).c_str() << " - ";
    auto val = node.symbol.c_str();
    if (sym2type_.contains(val)) add_def(sym2type_[val]);
    std::cout << sym2type_[val] << "\n";
}

} // namespace mim::plug::eqsat
