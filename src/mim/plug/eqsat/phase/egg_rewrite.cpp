#include "mim/plug/eqsat/phase/egg_rewrite.h"

#include "mim/def.h"
#include "mim/driver.h"

#include "mim/plug/eqsat/autogen.h"

#include "fe/assert.h"

namespace mim::plug::eqsat {

void EggRewrite::start() {
    std::ostringstream sexpr;
    std::cout << "started eqsat phase..\n";

    auto [rulesets, rules] = import_rules();
    std::cout << "imported rules and rulesets..\n";

    if (auto sexpr_backend = driver().backend("sexpr"))
        sexpr_backend(old_world(), sexpr);
    else
        error("EggRewrite: 'sexpr' emitter not loaded; try loading 'core' plugin");

    std::cout << "got the sexpr..\n";
    std::cout << sexpr.str() << "\n";

    // NOTE: The symbolic expression 'sexpr' will actually be a series
    // of symbolic expressions, one for each closed mutable Def.
    // Passing 'sexpr' into equality_saturate(sexpr: &str) will return a Vec<RewriteResult>
    // where each RewriteResult represents one rewritten symbolic expression
    // in the form of a recursive expression (a way of representing a symbolic expression as a list of nodes)
    auto rewrites = equality_saturate(sexpr.str(), rulesets);
    std::cout << "got the rewrite results (total: " << rewrites.size() << ")..\n";
    for (auto rewrite : rewrites)
        process(rewrite);

    std::cout << "recreated the world..\n\n";
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

        // TODO: set projections of variables (i.e. sigma vars)
        // auto projs = var->projs();
        // if (!projs.empty()) {
        //     for (auto proj : projs) {
        //         // proj->set("Helloo");
        //     }
        // }

        i++;
    }
}

void EggRewrite::init_var(MimNode node) {
    // NOTE: Assuming (var <name> <type>)
    // but this might not always be the case. (Anonymous vars)
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
    auto callee_sym = get_symbol(node.children[0]);
    auto callee_def = get_def(node.children[0]);
    auto arg        = get_def(node.children[1]);

    if (callee_sym != "") {
        // Case 1: (app <symbol> <arg>)
        if (callee_sym.starts_with("%")) {
            flags_t annex_id = sym2flags_[callee_sym];
            auto new_call    = new_world().call(annex_id, arg);
            add_def(new_call);
        } else {
            auto var     = get_var(callee_sym);
            auto new_app = new_world().app(var, arg);
            add_def(new_app);
        }
    } else if (callee_def != nullptr) {
        // Case 2: (app <node> <arg>)
        auto new_app = new_world().app(callee_def, arg);
        add_def(new_app);
    } else {
        fe::unreachable();
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

void EggRewrite::convert_arr(MimNode node) {
    auto arity = get_def(node.children[0]);
    auto type  = get_def(node.children[1]);

    auto new_arr = new_world().arr(arity, type);
    add_def(new_arr);
}

// (tuple <node1> <node2> <node3> ...)
// TODO: arg tuples of lambda headers shouldn't be added to the world
void EggRewrite::convert_tuple(MimNode node) {
    DefVec ops;
    for (auto child : node.children) {
        auto op_sym = get_symbol(child);
        auto op_def = op_sym != "" ? get_var(op_sym) : get_def(child);
        ops.push_back(op_def);
    }
    auto new_tuple = new_world().tuple(ops);
    add_def(new_tuple);
}

void EggRewrite::convert_extract(MimNode node) {
    auto tuple_sym = get_symbol(node.children[0]);
    auto tuple_def = get_def(node.children[0]);
    auto index     = get_def(node.children[1]);

    if (tuple_sym != "") {
        auto tuple = get_var(tuple_sym);
        // TODO: Somehow extract fails at the alpha equivalence check for index and tuple arity.
        // I guess if we are extracting from a sigma, we need to use the second extract api:
        // extract(tuple, arity, index) (this is what they do in ExtractExpr::emit)
        auto extract = new_world().extract(tuple, index);
        add_def(extract);
    } else if (tuple_def != nullptr) {
        auto extract = new_world().extract(tuple_def, index);
        add_def(extract);
    } else {
        fe::unreachable();
    }

    /*
      auto tuple_sym = get_symbol(node.children[0]);
      auto tuple_def = tuple_sym != "" ? get_var(tuple_sym) : get_def(node.children[0]);
      auto index_def = get_def(node.children[1]);

      std::cout << "Here 0\n";
      std::cout << tuple_sym << "\n";
      std::cout << tuple_def << "\n";
      std::cout << tuple_def->type() << "\n";
      if (tuple_def->type()->isa<Sigma>()) {
          std::cout << "Here 1\n";
          auto arity      = tuple_def->num_ops();
          auto index_node = res_[node.children[1]];
          if (index_node.kind == MimKind::Lit) {
              std::cout << "Here 2\n";
              auto index_num = get_num(index_node.children[0]);
              auto index_sym = get_symbol(index_node.children[0]);
              index_sym == "ff" ? index_num = 0 : index_sym == "tt" ? index_num = 1 : index_num = index_num;

              if (tuple_sym != "") {
                  std::cout << "Here 3\n";
                  std::cout << (u64)arity << " : " << (u64)index_num << "\n";
                  auto extract = new_world().extract(tuple_def, (u64)arity, (u64)index_num);
                  std::cout << "Here 4\n";
                  add_def(extract);
              } else if (tuple_def != nullptr) {
                  auto extract = new_world().extract(tuple_def, arity, index_num);
                  add_def(extract);
              } else {
                  fe::unreachable();
              }
          } else {
              // use extract(tuple_def, index_def) api
          }
      }
    */
}

void EggRewrite::convert_ins(MimNode node) {}

// (sigma (var <name1> <type1>) (var <name2> <type2>) ...) or (sigma <type1> <type2> ...)
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
    auto val = node.symbol.c_str();
    if (sym2type_.contains(val)) add_def(sym2type_[val]);
}

} // namespace mim::plug::eqsat
