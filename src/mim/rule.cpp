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

bool Rule::its_a_match(const Def* exp1, const Def* exp2) const {
    if (is_in_rule(exp1)) return false;
    // we don't rewrite the rewrites
    if (are_same_node(exp1, exp2)) {
        // should be ok if we do not have to infer the types
        // gotta assume that we have the same kind of node now
        // we assume all vars in exp2 are pattern matching meta variables
        // therefore they match everything (no equality pls)
        if (exp2->isa<Var>()) return true;
        // else we need to check for a match in all branches (except if no dependencies, then check equality)
        if (exp2->num_ops() == 0) return exp2 == exp1;
        if (exp2->num_ops() != exp1->num_ops()) return false;
        for (size_t i = 0; i < exp2->num_ops(); i++)
            if (!its_a_match(exp1->op(i), exp2->op(i))) return false;
        return true;
    }
    return false;
}

const Def* Rule::replace(const Def* expr) const { return Rule::replace(lhs(), expr); }

const Def* Rule::replace(const Def* exp1, const Def* exp2) const {
    const Def* res = exp2;
    if (auto v = exp1->isa<Var>()) {
        if (v->mut()->isa<Rule>()) {
            // base case, we bind the actual value to the meta var and replace it in rhs
            auto rw = VarRewriter(v, exp2);
            res     = rw.rewrite(rhs());
            return res;
        }
    }
    for (size_t i = 0; i < exp1->num_ops(); i++) {
        // recursively go find all meta vars in lhs
        res = replace(exp1->op(i), exp2->op(i));
    }
    return res;
}

} // namespace mim
