#include "mim/rule.h"

#include "mim/check.h"
#include "mim/def.h"
#include "mim/lam.h"
#include "mim/lattice.h"
#include "mim/rewrite.h"
#include "mim/tuple.h"
#include "mim/world.h"

namespace mim {

template<class T> bool same(const Def* exp1, const Def* exp2) {
    return !(exp1->isa<T>() == nullptr || exp2->isa<T>() == nullptr);
}

bool are_same_node(const Def* expr1, const Def* expr2) {
    return same<Lam>(expr1, expr2) || same<Lit>(expr1, expr2) || same<Axm>(expr1, expr2) || same<Var>(expr1, expr2)
        || same<Global>(expr1, expr2) || same<Proxy>(expr1, expr2) || same<Hole>(expr1, expr2)
        || same<Type>(expr1, expr2) || same<Univ>(expr1, expr2) || same<UInc>(expr1, expr2) || same<Pi>(expr1, expr2)
        || same<Lam>(expr1, expr2) || same<App>(expr1, expr2) || same<Sigma>(expr1, expr2) || same<Tuple>(expr1, expr2)
        || same<Extract>(expr1, expr2) || same<Insert>(expr1, expr2) || same<Arr>(expr1, expr2)
        || same<Pack>(expr1, expr2) || same<Join>(expr1, expr2) || same<Inj>(expr1, expr2) || same<Match>(expr1, expr2)
        || same<Top>(expr1, expr2) || same<Meet>(expr1, expr2) || same<Merge>(expr1, expr2) || same<Split>(expr1, expr2)
        || same<Bot>(expr1, expr2) || same<Uniq>(expr1, expr2) || same<Nat>(expr1, expr2) || same<Idx>(expr1, expr2);
}

bool is_in_rule(const Def* expr) {
    // are we inside a rule ?
    for (auto var : expr->free_vars()) {
        if (var->mut()->isa<Rule>()) {
            // var is introduced by a rule: this is inside of a body of a rule
            return true;
        }
    }
    return false;
}

bool Rule::its_a_match(const Def* expr) const {
    if (is_in_rule(expr)) return false;
    return Rule::its_a_match(expr, lhs());
}

bool Rule::its_a_match(const Def* exp1, const Def* exp2, std::map<const Def*, const Def*> already_seen) const {
    if (is_in_rule(exp1)) return false;
    // if (exp1->type() != exp2->type()) return false;
    if (exp1->type() != nullptr && exp2->type() != nullptr)
        if (!its_a_match(exp1->type(), exp2->type(), already_seen)) return false;
    // we don't rewrite the rewrites

    // we assume all vars in exp2 are pattern matching meta variables
    // therefore they match everything (no equality pls)
    if (exp2->isa<Var>()) {
        if (already_seen.contains(exp2)) return exp1 == already_seen[exp2];
        if (exp2->as<Var>()->mut()->isa<Rule>()) {
            already_seen[exp2] = exp1;
            return true;
        }
        return exp1 == exp2;
        // we want to have 2 bound variables that are equal
    }
    if (auto e2 = exp2->isa<Extract>()) {
        if (auto v = e2->tuple()->isa<Var>()) {
            // we have a var in a tuple
            if (already_seen.contains(e2)) return exp1 == already_seen[e2];
            if (v->mut()->isa<Rule>()) {
                already_seen[e2] = exp1;
                return true;
            }
            return exp1 == exp2;
        }
    }

    if (are_same_node(exp1, exp2)) {
        // should be ok if we do not have to infer the types
        // gotta assume that we have the same kind of node now

        // else we need to check for a match in all branches (except if no dependencies, then check equality)
        if (auto l1 = exp1->isa<Lit>(); auto l2 = exp2->isa<Lit>()) return l1->get() == l2->get();
        if (auto a1 = exp1->isa<Axm>(); auto a2 = exp2->isa<Axm>()) return a1->flags() == a2->flags();
        if (exp2->num_ops() == 0) return true;
        if (exp2->num_ops() != exp1->num_ops()) return false;
        for (size_t i = 0; i < exp2->num_ops(); i++)
            if (!its_a_match(exp1->op(i), exp2->op(i), already_seen)) return false;
        return true;
    }
    return false;
}

const Def* Rule::replace(const Def* expr) const {
    std::map<std::pair<const Def*, size_t>, const Def*> var2replace;
    auto v = Rule::replace_(lhs(), expr, var2replace);
    // the dic is now populated with everything
    std::vector<std::pair<const Def*, size_t>> tuple_of_args;
    for (auto v : var2replace) tuple_of_args.push_back(std::make_pair(v.second, v.first.second));
    std::sort(tuple_of_args.begin(), tuple_of_args.end(),
              [&](std::pair<const Def*, size_t> a, std::pair<const Def*, size_t> b) { return a.second < b.second; });
    std::vector<const Def*> fin;
    for (auto p : tuple_of_args) fin.push_back(p.first);
    auto truc  = world().tuple(fin);
    auto rw    = VarRewriter(v, truc);
    auto condi = rw.rewrite(condition());
    if (condi == world().lit_tt()) return rw.rewrite(rhs());
    return expr;
}

const Var* Rule::replace_(const Def* exp1,
                          const Def* exp2,
                          std::map<std::pair<const Def*, size_t>, const Def*>& var2replace) const {
    const Var* var_of_var;
    if (exp1 == exp2) return nullptr;
    if (auto v = exp1->isa<Var>()) {
        if (v->mut()->isa<Rule>()) {
            // base case, we bind the actual value to the meta var and replace it in rhs
            var2replace[std::make_pair(v, -1)] = exp2;
            return v;
        }
    }
    if (auto e = exp1->isa<Extract>()) {
        if (auto v = e->tuple()->isa<Var>()) {
            if (v->mut()->isa<Rule>()) {
                var2replace[std::make_pair(e, e->index()->as<Lit>()->get())] = exp2;
                return v;
            }
        }
    }
    replace_(exp1->type(), exp2->type(), var2replace);
    for (size_t i = 0; i < exp1->num_ops(); i++) {
        // recursively go find all meta vars in lhs
        auto v = replace_(exp1->op(i), exp2->op(i), var2replace);
        if (v) var_of_var = v;
    }
    return var_of_var;
}

} // namespace mim
