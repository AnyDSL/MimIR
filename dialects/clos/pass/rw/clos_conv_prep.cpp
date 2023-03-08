#include "dialects/clos/pass/rw/clos_conv_prep.h"

#include "thorin/pass/fp/eta_exp.h"

#include "dialects/clos/clos.h"

namespace thorin::clos {

// FIXME: these guys do not work if another pass rewrites curr_nom()'s body
static bool isa_cont(const App* body, const Def* def, size_t i) {
    return body->callee_type()->is_returning() && body->arg() == def && i == def->num_ops() - 1;
}

static const Def* isa_br(const App* body, const Def* def) {
    if (!body->callee_type()->is_cn()) return nullptr;
    auto proj = body->callee()->isa<Extract>();
    return (proj && proj->tuple() == def && proj->tuple()->isa<Tuple>()) ? proj->tuple() : nullptr;
}

static bool isa_callee_br(const App* body, const Def* def, size_t i) {
    if (!body->callee_type()->is_cn()) return false;
    return isa_callee(def, i) || isa_br(body, def);
}

static Lam* isa_retvar(const Def* def) {
    if (auto [var, lam] = ca_isa_var<Lam>(def); var && lam && var == lam->ret_var()) return lam;
    return nullptr;
}

Lam* ClosConvPrep::scope(Lam* lam) {
    if (eta_exp_) lam = eta_exp_->new2old(lam);
    return lam2fscope_[lam];
}

void ClosConvPrep::enter() {
    if (curr_nom()->type()->is_returning()) {
        lam2fscope_[curr_nom()] = curr_nom();
        world().DLOG("scope {} -> {}", curr_nom(), curr_nom());
        scope_ = std::make_unique<Scope>(curr_nom());
        for (auto def : scope_->bound()) {
            assert(def);
            if (auto bb_lam = def->isa_nom<Lam>(); bb_lam && bb_lam->is_basicblock()) {
                world().DLOG("scope {} -> {}", bb_lam, curr_nom());
                lam2fscope_[bb_lam] = curr_nom();
            }
        }
    }

    auto body = curr_nom()->body()->isa<App>();
    // Skip if the nominal is already wrapped or the body is undefined/no continuation.
    ignore_ = !(body && body->callee_type()->is_cn()) || wrapper_.contains(curr_nom());
}

const App* ClosConvPrep::rewrite_arg(const App* app) {
    auto& w  = world();
    auto arg = app->arg();

    if (arg->isa<Var>()) return app;

    for (auto i = 0u; i < arg->num_projs(); i++) {
        auto op = arg->proj(i);

        auto refine = [&](const Def* new_op) {
            const Def* args;
            if (arg == op) {
                args = new_op;
            } else {
                args = arg->refine(i, new_op);
            }
            return app->refine(1, args)->as<App>();
        };

        if (auto lam = isa_retvar(op); lam && from_outer_scope(lam)) {
            w.DLOG("found return var from enclosing scope: {}", op);
            return refine(eta_wrap(op, attr::freeBB)->set(sym_.free_ret));
        }
        if (auto bb_lam = op->isa_nom<Lam>(); bb_lam && bb_lam->is_basicblock() && from_outer_scope(bb_lam)) {
            w.DLOG("found BB from enclosing scope {}", op);
            return refine(thorin::clos::op(attr::freeBB, op));
        }
        if (isa_cont(app, arg, i)) {
            if (match<attr>(attr::ret, op) || isa_retvar(op)) {
                return app;
            } else if (auto contlam = op->isa_nom<Lam>()) {
                return refine(thorin::clos::op(attr::ret, contlam));
            } else {
                auto wrapper = eta_wrap(op, attr::ret)->set(sym_.eta_cont);
                w.DLOG("eta expanded return cont: {} -> {}", op, wrapper);
                return refine(wrapper);
            }
        }

        if (!isa_callee_br(app, arg, i)) {
            if (auto bb_lam = op->isa_nom<Lam>(); bb_lam && bb_lam->is_basicblock()) {
                w.DLOG("found firstclass use of BB: {}", bb_lam);
                return refine(thorin::clos::op(attr::fstclassBB, bb_lam));
            }
            // TODO: If EtaRed eta-reduces branches, we have to wrap them again!
            if (isa_retvar(op)) {
                w.DLOG("found firstclass use of return var: {}", op);
                return refine(eta_wrap(op, attr::fstclassBB)->set(sym_.fstclass_ret));
            }
        }
    }

    return app;
}

const App* ClosConvPrep::rewrite_callee(const App* app) {
    auto& w = world();
    if (app->callee_type()->is_cn()) {
        if (auto br = app->callee()->isa<Extract>()) {
            auto branches = br->tuple();
            // Eta-Expand branches
            if (branches->isa<Tuple>() && branches->type()->isa<Arr>()) {
                for (auto i = 0u; i < branches->num_ops(); i++) {
                    if (!branches->op(i)->isa_nom<Lam>()) {
                        auto wrapper = eta_wrap(branches->op(i), attr::bot)->set(sym_.eta_br);
                        w.DLOG("eta wrap branch: {} -> {}", branches->op(i), wrapper);
                        branches = branches->refine(i, wrapper);
                    }
                }
                return app->refine(0, app->callee()->refine(0, branches))->as<App>();
            }
        }
    }

    return app;
}

const Def* ClosConvPrep::rewrite(const Def* def) {
    if (ignore_ || match<attr>(def)) return def;

    if (auto app = def->isa<App>(); app) {
        app = rewrite_arg(app);
        app = rewrite_callee(app);
        return app;
    }

    return def;
}

} // namespace thorin::clos
