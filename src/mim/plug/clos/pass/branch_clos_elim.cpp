#include "mim/plug/clos/pass/branch_clos_elim.h"

#include "mim/plug/clos/clos.h"

namespace mim::plug::clos {

namespace {

std::tuple<std::vector<ClosLit>, const Def*> isa_branch(const Def* callee) {
    if (auto closure_proj = callee->isa<Extract>()) {
        auto inner_proj = closure_proj->tuple()->isa<Extract>();
        if (inner_proj && inner_proj->tuple()->isa<Tuple>() && isa_clos_type(inner_proj->type())) {
            auto branches = std::vector<ClosLit>();
            for (auto op : inner_proj->tuple()->ops())
                if (auto c = isa_clos_lit(op))
                    branches.push_back(std::move(c));
                else
                    return {};
            return {branches, inner_proj->index()};
        }
    }
    return {};
}

} // namespace

const Def* BranchClosElim::rewrite(const Def* def) {
    auto& w  = world();
    auto app = def->isa<App>();
    if (!app || !Pi::isa_cn(app->callee_type())) return def;

    if (auto [branches, index] = isa_branch(app->callee()); index) {
        w.DLOG("FLATTEN BRANCH {}", app->callee());
        auto new_branches = w.tuple(DefVec(branches.size(), [&](auto i) {
            auto c                 = branches[i];
            auto [entry, inserted] = branch2dropped_.emplace(c, nullptr);
            auto& dropped_lam      = entry->second;
            if (inserted || !dropped_lam) {
                auto clam     = c.fnc_as_lam();
                dropped_lam   = clam->stub(clos_type_to_pi(c.type()));
                auto new_vars = clos_insert_env(c.env(), dropped_lam->var());
                dropped_lam->set(clam->reduce(new_vars));
            }
            return dropped_lam;
        }));
        return w.app(w.extract(new_branches, index), clos_remove_env(app->arg()));
    }

    return def;
}

}; // namespace mim::plug::clos
