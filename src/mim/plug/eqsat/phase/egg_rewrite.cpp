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

    for (uint32_t id = 0; id < res_.size(); ++id)
        init(id);

    for (uint32_t id = 0; id < res_.size(); ++id)
        convert(id);
}

const Def* EggRewrite::init(uint32_t id) {
    auto node = res_[id];

    const Def* res = nullptr;
    switch (node.kind) {
        case MimKind::Fun: res = init_fun(id, node); break;
        case MimKind::Lam: res = init_lam(id, node); break;
        case MimKind::Con: res = init_con(id, node); break;
        default: break;
    }

    return added_[id] = res;
}

const Def* EggRewrite::init_fun(uint32_t id, MimNode node) { return nullptr; }
const Def* EggRewrite::init_lam(uint32_t id, MimNode node) { return nullptr; }

const Def* EggRewrite::init_con(uint32_t id, MimNode node) {
    std::cout << "init - current node(" << id << "): " << mim_node_str(node).c_str() << "\n";
    auto con_name = get_symbol(node.children[1]);
    auto domain   = get_node(MimKind::Tuple, node.children[2]);

    DefVec var_types;
    std::vector<std::string> var_names;
    for (auto child : domain.children) {
        auto var = res_[child];
        std::string var_name;
        const Def* var_type;
        if (var.kind == MimKind::Var) {
            var_name = get_symbol(var.children[0]);
            var_type = convert(var.children[1], true);
        } else {
            var_name = "";
            var_type = convert(child, true);
        }
        var_names.push_back(var_name);
        var_types.push_back(var_type);
    }

    auto new_con = new_world().mut_con(var_types)->set(con_name.c_str());
    register_lam(con_name, new_con);

    for (int i = 0; auto var : new_con->vars()) {
        auto var_name = var_names[i];
        var->set(var_name);
        register_var(var_name, var);
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
    return new_con;
}

const Def* EggRewrite::convert(uint32_t id, bool recurse) {
    // TODO: skip out on conversion if id already in added_
    // - don't skip for lambdas because they will be added in init()
    //   but their bodies will only be set in convert()
    auto node = res_[id];

    if (recurse)
        for (uint32_t child : node.children)
            convert(child, recurse);

    const Def* res = nullptr;
    switch (node.kind) {
        case MimKind::Let: res = convert_let(id, node); break;
        case MimKind::Fun: res = convert_fun(id, node); break;
        case MimKind::Lam: res = convert_lam(id, node); break;
        case MimKind::Con: res = convert_con(id, node); break;
        case MimKind::App: res = convert_app(id, node); break;
        case MimKind::Var: res = convert_var(id, node); break;
        case MimKind::Lit: res = convert_lit(id, node); break;
        case MimKind::Pack: res = convert_pack(id, node); break;
        case MimKind::Tuple: res = convert_tuple(id, node); break;
        case MimKind::Extract: res = convert_extract(id, node); break;
        case MimKind::Ins: res = convert_ins(id, node); break;
        case MimKind::Bot: res = convert_bot(id, node); break;
        case MimKind::Top: res = convert_top(id, node); break;
        case MimKind::Arr: res = convert_arr(id, node); break;
        case MimKind::Sigma: res = convert_sigma(id, node); break;
        case MimKind::Cn: res = convert_cn(id, node); break;
        case MimKind::Pi: res = convert_pi(id, node); break;
        case MimKind::Idx: res = convert_idx(id, node); break;
        case MimKind::Hole: res = convert_hole(id, node); break;
        case MimKind::Type: res = convert_type(id, node); break;
        case MimKind::Num: res = convert_num(id, node); break;
        case MimKind::Symbol: res = convert_symbol(id, node); break;
        default: break;
    }

    return added_[id] = res;
}

// (let <name> <definition> <expression>)
const Def* EggRewrite::convert_let(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto expr = get_def(node.children[2]);
    std::cout << expr << "\n";
    return expr;
}

// (fun <extern> <name> <domain> <codomain> [<filter>] [<body>])
const Def* EggRewrite::convert_fun(uint32_t id, MimNode node) { return nullptr; }
// (lam <extern> <name> <domain> <codomain> [<filter>] [<body>])
const Def* EggRewrite::convert_lam(uint32_t id, MimNode node) { return nullptr; }

// (con <extern> <name> <domain> [<filter>] [<body>])
const Def* EggRewrite::convert_con(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto con = get_def(id)->as_mut<Lam>();

    auto is_extern = get_symbol(node.children[0]);
    if (is_extern == "extern") con->externalize();

    const int CON_DECL_SIZE = 3;
    const int CON_DEF_SIZE  = 5;
    if (node.children.size() == CON_DEF_SIZE) {
        auto filter = get_def(node.children[3]);
        auto body   = get_def(node.children[4]);
        con->set_filter(filter);
        con->set_body(body);
    } else if (node.children.size() == CON_DECL_SIZE)
        con->set_filter(false);

    std::cout << con << "\n";
    return con;
}

// (app <callee> <arg>)
const Def* EggRewrite::convert_app(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto callee  = get_def(node.children[0]);
    auto arg     = get_def(node.children[1]);
    auto new_app = new_world().app(callee, arg);
    std::cout << new_app << "\n";
    return new_app;
}

// (var <name> <type>)
const Def* EggRewrite::convert_var(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto name = get_symbol(node.children[0]);
    auto var  = get_var(name);
    std::cout << var << "\n";
    return var;
}

// (lit <val> [<type>])
const Def* EggRewrite::convert_lit(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto lit_sym = get_symbol(node.children[0]);
    auto lit_num = get_num(node.children[0]);
    const Def* new_lit;

    if (lit_sym == "ff") {
        // Case 1: (lit tt)
        new_lit = new_world().lit_ff();
    } else if (lit_sym == "tt") {
        // Case 1: (lit ff)
        new_lit = new_world().lit_tt();
    } else if (node.children.size() > 1) {
        // Case 2: (lit <val> <type>)
        auto lit_type = get_def(node.children[1]);
        new_lit       = new_world().lit(lit_type, lit_num);
    } else {
        // Case 3: (lit <val>)
        new_lit = new_world().lit_nat(lit_num);
    }
    std::cout << new_lit << "\n";
    return new_lit;
}

// (pack <arity> <body>)
const Def* EggRewrite::convert_pack(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto arity = get_def(node.children[0]);
    auto body  = get_def(node.children[1]);

    auto new_pack = new_world().pack(arity, body);
    std::cout << new_pack << "\n";
    return new_pack;
}

// (tuple <node1> <node2> ...)
// TODO: arg tuples of lambda headers shouldn't be added to the world
const Def* EggRewrite::convert_tuple(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    DefVec ops;
    for (auto child : node.children) {
        auto op = get_def(child);
        ops.push_back(op);
    }
    auto new_tuple = new_world().tuple(ops);
    std::cout << new_tuple << "\n";
    return new_tuple;
}

// (extract <tuple> <index>)
const Def* EggRewrite::convert_extract(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto tuple = get_def(node.children[0]);
    auto index = get_def(node.children[1]);

    auto new_extract = new_world().extract(tuple, index);
    std::cout << new_extract << "\n";
    return new_extract;
}

// (ins <tuple> <index> <value>)
const Def* EggRewrite::convert_ins(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto tuple = get_def(node.children[0]);
    auto index = get_def(node.children[1]);
    auto value = get_def(node.children[2]);

    auto new_insert = new_world().insert(tuple, index, value);
    std::cout << new_insert << "\n";
    return new_insert;
}

// (bot <name> <type>)
const Def* EggRewrite::convert_bot(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto name = get_symbol(node.children[0]);
    auto type = get_def(node.children[1]);

    auto new_bot = new_world().bot(type);
    if (name != "⊥") register_var(name, new_bot);

    std::cout << new_bot << "\n";
    return new_bot;
}
// (top <name> <type>)
const Def* EggRewrite::convert_top(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto name = get_symbol(node.children[0]);
    auto type = get_def(node.children[1]);

    auto new_top = new_world().top(type);
    if (name != "T") register_var(name, new_top);

    std::cout << new_top << "\n";
    return new_top;
}

// (arr <arity> <body>)
const Def* EggRewrite::convert_arr(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto arity = get_def(node.children[0]);
    auto body  = get_def(node.children[1]);

    auto new_arr = new_world().arr(arity, body);
    std::cout << new_arr << "\n";
    return new_arr;
}

// (sigma (var <name1> <type1>) (var <name2> <type2>) ...)
//  or
// (sigma <type1> <type2> ...)
const Def* EggRewrite::convert_sigma(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
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
    std::cout << new_sigma << "\n";
    return new_sigma;
}

// (cn <domain>)
const Def* EggRewrite::convert_cn(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto domain = get_def(node.children[0]);
    auto new_cn = new_world().cn(domain);
    std::cout << new_cn << "\n";
    return new_cn;
}

// (pi <domain> <codomain>)
const Def* EggRewrite::convert_pi(uint32_t id, MimNode node) { return nullptr; }

// (idx <size>)
const Def* EggRewrite::convert_idx(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto size    = get_def(node.children[0]);
    auto new_idx = new_world().type_idx(size);
    std::cout << new_idx << "\n";
    return new_idx;
}

// (hole ...)
const Def* EggRewrite::convert_hole(uint32_t id, MimNode node) { return nullptr; }
// (type <level>)
const Def* EggRewrite::convert_type(uint32_t id, MimNode node) { return nullptr; }

// <i64>
const Def* EggRewrite::convert_num(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << "\n";
    return nullptr;
}

// <string>
const Def* EggRewrite::convert_symbol(uint32_t id, MimNode node) {
    std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    auto def = get_def(id);
    std::cout << def << "\n";
    return def;
}

} // namespace mim::plug::eqsat
