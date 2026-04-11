#include <cstdint>

#include <mim/plug/eqsat/eqsat.h>
#include <mim/plug/eqsat/phase/egg_rewrite.h>

#include "mim/def.h"
#include "mim/driver.h"

namespace mim::plug::eqsat {

void EggRewrite::start() {
    auto [rulesets, cost_fn] = import_config();

    std::ostringstream sexpr;
    if (auto sexpr_backend = driver().backend("sexpr"))
        sexpr_backend(old_world(), sexpr);
    else
        error("EggRewrite: 'sexpr' emitter not loaded; try loading 'core' plugin");

    // std::cout << pretty(sexpr.str(), 80).c_str() << "\n";

    auto rewrites = equality_saturate(sexpr.str(), rulesets, cost_fn);

    // Initially creates lambdas and their variables as Def's in the new world but doesn't set their bodies yet.
    // This is required so that other terms can later refer to lambdas and variables by name
    // as in (app foo (lit 0))) where 'foo' could be a lambda created in this inital stage.
    for (auto rewrite : rewrites) {
        added_ = {};
        res_   = rewrite.value;
        for (uint32_t id = 0; id < res_.size(); id++)
            init(id, true, false);
    }

    for (auto rewrite : rewrites) {
        added_ = {};
        res_   = rewrite.value;

        // Then creates let-bindings that may already refer to lambdas and variables defined earlier.
        // For this, we have to iterate over the rewrite in reverse-order to ensure that
        // let bindings are initalized from outermost to innermost because inner let bindings
        // may depend on outer let bindings.
        for (uint32_t id = res_.size() - 1; id > 0; id--)
            init(id, false, true);
        // Converts remaining nodes to Def's in the new world and sets the bodies of the previously created lambdas.
        for (uint32_t id = 0; id < res_.size(); id++)
            convert(id);
    }

    swap(old_world(), new_world());
}

std::pair<rust::Vec<RuleSet>, CostFn> EggRewrite::import_config() {
    // Internalize eqsat ruleset config lambda (lam with signature [] -> %eqsat.Ruleset)
    DefVec lams;
    for (auto def : old_world().externals().mutate()) {
        if (auto lam = def->isa<Lam>()) {
            if (Axm::isa<eqsat::Ruleset>(lam->ret_dom()) || Axm::isa<eqsat::CostFun>(lam->ret_dom())) {
                lams.push_back(lam);
                def->internalize();
            }
        }
    }

    // Import predefined rulesets and cost function from config lambdas
    rust::Vec<RuleSet> rulesets;
    CostFn cost_fn = CostFn::AstSize;
    for (auto lam : lams) {
        auto body = lam->as<Lam>()->body();
        if (auto body_app = body->isa<App>()) {
            if (auto ruleset_config = Axm::isa<eqsat::rulesets>(body_app->arg())) {
                for (auto ruleset : ruleset_config->args())
                    if (Axm::isa<eqsat::core>(ruleset))
                        rulesets.push_back(RuleSet::Core);
                    else if (Axm::isa<eqsat::math>(ruleset))
                        rulesets.push_back(RuleSet::Math);

            } else if (Axm::isa<eqsat::AstSize>(body_app->arg())) {
                cost_fn = CostFn::AstSize;
            } else if (Axm::isa<eqsat::AstDepth>(body_app->arg())) {
                cost_fn = CostFn::AstDepth;
            }
        }
    }

    return {rulesets, cost_fn};
}

const Def* EggRewrite::init(uint32_t id, bool lambdas, bool bindings) {
    auto node = get_node_unsafe(id);

    // std::cout << "init - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    const Def* res = nullptr;
    switch (node.kind) {
        case MimKind::Lam: res = lambdas ? init_lam(id, node) : nullptr; break;
        case MimKind::Con: res = lambdas ? init_con(id, node) : nullptr; break;
        case MimKind::Let: res = bindings ? init_let(id, node) : nullptr; break;
        default: break;
    }
    // std::cout << res << "\n";
    return added_[id] = res;
}

// TODO: implement
// (lam <extern> <name> <domain> <codomain> [<filter>] [<body>])
const Def* EggRewrite::init_lam(uint32_t id, MimNode node) { return nullptr; }

// (con <extern> <name> <domain> [<filter>] [<body>])
const Def* EggRewrite::init_con(uint32_t id, MimNode node) {
    DefVec var_types;
    std::vector<std::string> var_names;
    auto domain = get_node(MimKind::Sigma, node.children[2]);
    for (auto child : domain.children) {
        auto var = get_node_unsafe(child);
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

    auto con_name       = get_symbol(node.children[1]);
    auto con_name_nouid = remove_uid(con_name);
    auto new_con        = new_world().mut_con(var_types);
    new_con->set(con_name_nouid);
    register_lam(con_name, new_con);

    for (int i = 0; auto var_name : var_names) {
        if (!var_name.empty()) {
            // TODO: Does this cover all cases or do we also need the same check for Sigma?
            // We needed this check specifically in case the only var of a lambda is an array
            // because we would otherwise get the i'th projection of the array instead.
            auto var            = var_types.size() == 1 && var_types[i]->isa<Arr>() ? new_con->var() : new_con->var(i);
            auto var_name_nouid = remove_uid(var_name);
            var->set(var_name_nouid);
            if (!var_name.empty()) register_var(var_name, var);
        }
        i++;
    }

    // We need another recursive call to convert() for each var to ensure (nested) var projections
    // are set and registered inside of convert_var()
    for (auto child : domain.children)
        convert(child, true);

    return new_con;
}

// (let <name> <definition> <expression>)
const Def* EggRewrite::init_let(uint32_t id, MimNode node) {
    // If the let-binding is for a lambda, this lambda will already have been
    // created, set and registered via init_lam/con and thus we can skip it.
    auto let_def = get_def(node.children[0]);
    if (let_def != nullptr) return nullptr;

    auto name       = get_symbol(node.children[0]);
    auto name_nouid = remove_uid(name);
    auto def        = convert(node.children[1], true);
    def->set(name_nouid);
    register_var(name, def);

    return nullptr;
}

const Def* EggRewrite::convert(uint32_t id, bool recurse) {
    auto node = get_node_unsafe(id);

    if (recurse)
        for (uint32_t child : node.children)
            convert(child, recurse);

    // If a Def has been created for a node, we do not need to revisit its convert function.
    // The only exceptions are lambdas and variables, since their Defs already get created in
    // the init functions and need to be further modified in their convert functions.
    const Def* res                  = added_[id];
    const std::set<MimKind> revisit = {MimKind::Lam, MimKind::Con, MimKind::Var};
    if (res != nullptr && !revisit.contains(node.kind)) return res;

    // std::cout << "convert - current node(" << id << "): " << mim_node_str(node).c_str() << " - ";
    switch (node.kind) {
        case MimKind::Let: res = convert_let(id, node); break;
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

    // std::cout << res << "\n";
    return added_[id] = res;
}

// (let <name> <definition> <expression>)
const Def* EggRewrite::convert_let(uint32_t id, MimNode node) {
    auto expr = get_def(node.children[2]);
    return expr;
}

// TODO: implement
// (lam <extern> <name> <domain> <codomain> [<filter>] [<body>])
const Def* EggRewrite::convert_lam(uint32_t id, MimNode node) { return nullptr; }

// (con <extern> <name> <domain> [<filter>] [<body>])
const Def* EggRewrite::convert_con(uint32_t id, MimNode node) {
    const int CON_DECL_SIZE = 3;
    const int CON_DEF_SIZE  = 5;

    auto con = get_def(node.children[1])->as_mut<Lam>();

    auto is_extern = get_symbol(node.children[0]);
    if (is_extern == "extern") con->externalize();

    if (node.children.size() == CON_DEF_SIZE) {
        auto filter = get_def(node.children[3]);
        auto body   = get_def(node.children[4]);
        con->set_filter(filter);
        con->set_body(body);
    } else if (node.children.size() == CON_DECL_SIZE)
        con->set_filter(false);

    return con;
}

// (app <callee> <arg>)
const Def* EggRewrite::convert_app(uint32_t id, MimNode node) {
    auto callee  = get_def(node.children[0]);
    auto arg     = get_def(node.children[1]);
    auto new_app = new_world().app(callee, arg);
    return new_app;
}

// (var <name> <type>)
const Def* EggRewrite::convert_var(uint32_t id, MimNode node) {
    auto var      = get_def(node.children[0]);
    auto var_type = get_node_unsafe(node.children[1]);

    if (var && var_type.kind == MimKind::Sigma) {
        for (nat_t i = 0; uint32_t sigma_child_id : var_type.children) {
            auto sigma_child = get_node_unsafe(sigma_child_id);
            if (sigma_child.kind == MimKind::Var) {
                auto proj_name       = get_symbol(sigma_child.children[0]);
                auto proj_name_nouid = remove_uid(proj_name);
                auto sigma_proj      = var->proj(i);
                sigma_proj->set(proj_name_nouid);
                register_var(proj_name, sigma_proj);
            }
            i++;
        }
    }

    return var;
}

// (lit <val> [<type>])
const Def* EggRewrite::convert_lit(uint32_t id, MimNode node) {
    auto lit_def = get_def(node.children[0]);
    if (lit_def != nullptr) return lit_def;

    const Def* new_lit;
    auto lit_val = get_num(node.children[0]);
    if (node.children.size() > 1) {
        // Case 1: (lit <val> <type>)
        auto lit_type = get_def(node.children[1]);
        new_lit       = new_world().lit(lit_type, lit_val);
    } else {
        // Case 2: (lit <val>)
        new_lit = new_world().lit_nat(lit_val);
    }
    return new_lit;
}

// (pack <arity> <body>)
const Def* EggRewrite::convert_pack(uint32_t id, MimNode node) {
    auto arity    = get_def(node.children[0]);
    auto body     = get_def(node.children[1]);
    auto new_pack = new_world().pack(arity, body);
    return new_pack;
}

// (tuple <node1> <node2> ...)
const Def* EggRewrite::convert_tuple(uint32_t id, MimNode node) {
    DefVec ops;
    for (auto child : node.children) {
        auto op = get_def(child);
        ops.push_back(op);
    }
    auto new_tuple = new_world().tuple(ops);
    return new_tuple;
}

// (extract <tuple> <index>)
const Def* EggRewrite::convert_extract(uint32_t id, MimNode node) {
    auto tuple       = get_def(node.children[0]);
    auto index       = get_def(node.children[1]);
    auto new_extract = new_world().extract(tuple, index);
    return new_extract;
}

// (ins <tuple> <index> <value>)
const Def* EggRewrite::convert_ins(uint32_t id, MimNode node) {
    auto tuple      = get_def(node.children[0]);
    auto index      = get_def(node.children[1]);
    auto value      = get_def(node.children[2]);
    auto new_insert = new_world().insert(tuple, index, value);
    return new_insert;
}

// (bot <type>)
const Def* EggRewrite::convert_bot(uint32_t id, MimNode node) {
    auto type    = get_def(node.children[0]);
    auto new_bot = new_world().bot(type);
    return new_bot;
}
// (top <type>)
const Def* EggRewrite::convert_top(uint32_t id, MimNode node) {
    auto type    = get_def(node.children[0]);
    auto new_top = new_world().top(type);
    return new_top;
}

// (arr <arity> <body>)
const Def* EggRewrite::convert_arr(uint32_t id, MimNode node) {
    auto arity   = get_def(node.children[0]);
    auto body    = get_def(node.children[1]);
    auto new_arr = new_world().arr(arity, body);
    return new_arr;
}

// (sigma (var <name1> <type1>) (var <name2> <type2>) ...)
//  or
// (sigma <type1> <type2> ...)
const Def* EggRewrite::convert_sigma(uint32_t id, MimNode node) {
    DefVec ops;
    for (auto child : node.children) {
        auto op = get_node_unsafe(child);
        if (op.kind == MimKind::Var) {
            auto var_type = get_def(op.children[1]);
            ops.push_back(var_type);
        } else {
            auto type = get_def(child);
            ops.push_back(type);
        }
    }

    auto new_sigma = new_world().sigma(ops);
    return new_sigma;
}

// (cn <domain>)
const Def* EggRewrite::convert_cn(uint32_t id, MimNode node) {
    auto domain = get_def(node.children[0]);
    auto new_cn = new_world().cn(domain);
    return new_cn;
}

// (pi <domain> <codomain>)
const Def* EggRewrite::convert_pi(uint32_t id, MimNode node) {
    auto domain   = get_def(node.children[0]);
    auto codomain = get_def(node.children[1]);
    auto new_pi   = new_world().pi(domain, codomain);
    return new_pi;
}

// (idx <size>)
const Def* EggRewrite::convert_idx(uint32_t id, MimNode node) {
    auto size    = get_def(node.children[0]);
    auto new_idx = new_world().type_idx(size);
    return new_idx;
}

// (hole <type>)
const Def* EggRewrite::convert_hole(uint32_t id, MimNode node) {
    auto type_    = get_def(node.children[0]);
    auto new_hole = new_world().mut_hole(type_);
    return new_hole;
}

// (type <level>)
const Def* EggRewrite::convert_type(uint32_t id, MimNode node) {
    auto level    = get_def(node.children[0]);
    auto new_type = new_world().type(level);
    return new_type;
}

// <i64>
const Def* EggRewrite::convert_num(uint32_t id, MimNode node) { return nullptr; }

// <string>
const Def* EggRewrite::convert_symbol(uint32_t id, MimNode node) {
    auto def = get_def(id);
    return def;
}

} // namespace mim::plug::eqsat
