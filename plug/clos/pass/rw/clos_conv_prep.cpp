#include "plug/clos/pass/rw/clos_conv_prep.h"

#include "thorin/pass/fp/eta_exp.h"

#include "plug/clos/clos.h"

namespace thorin::clos {

namespace {
// FIXME: these guys do not work if another pass rewrites curr_mut()'s body
bool isa_cont(const App* body, Ref def, size_t i) {
    return Pi::isa_returning(body->callee_type()) && body->arg() == def && i == def->num_ops() - 1;
}

Ref isa_br(const App* body, Ref def) {
    if (!Pi::isa_cn(body->callee_type())) return nullptr;
    auto proj = body->callee()->isa<Extract>();
    return (proj && proj->tuple() == def && proj->tuple()->isa<Tuple>()) ? proj->tuple() : nullptr;
}

bool isa_callee_br(const App* body, Ref def, size_t i) {
    if (!Pi::isa_cn(body->callee_type())) return false;
    return isa_callee(def, i) || isa_br(body, def);
}

Lam* isa_retvar(Ref def) {
    if (auto [var, lam] = ca_isa_var<Lam>(def); var && lam && var == lam->ret_var()) return lam;
    return nullptr;
}

} // namespace

Lam* ClosConvPrep::scope(Lam* lam) {
    if (eta_exp_) lam = eta_exp_->new2old(lam);
    return lam2fscope_[lam];
}

void ClosConvPrep::enter() {
    if (Pi::isa_returning(curr_mut())) {
        lam2fscope_[curr_mut()] = curr_mut();
        world().DLOG("scope {} -> {}", curr_mut(), curr_mut());
        scope_ = std::make_unique<Scope>(curr_mut());
        for (auto def : scope_->bound()) {
            assert(def);
            if (auto bb_lam = Lam::isa_mut_basicblock(def)) {
                world().DLOG("scope {} -> {}", bb_lam, curr_mut());
                lam2fscope_[bb_lam] = curr_mut();
            }
        }
    }

    auto body = curr_mut()->body()->isa<App>();
    // Skip if the mutable is already wrapped or the body is undefined/no continuation.
    ignore_ = !(body && Pi::isa_cn(body->callee_type())) || wrapper_.contains(curr_mut());
}

const App* ClosConvPrep::rewrite_arg(const App* app) {
    auto& w  = world();
    auto arg = app->arg();

    if (arg->isa<Var>()) return app;

    for (auto i = 0u; i < arg->num_projs(); i++) {
        auto op = arg->proj(i);

        auto refine = [&](Ref new_op) {
            Ref args;
            if (arg == op)
                args = new_op;
            else
                args = arg->refine(i, new_op);
            return app->refine(1, args)->as<App>();
        };

        if (auto lam = isa_retvar(op); lam && from_outer_scope(lam)) {
            w.DLOG("found return var from enclosing scope: {}", op);
            return refine(eta_wrap(op, attr::freeBB)->set("free_ret"));
        }
        if (auto bb_lam = Lam::isa_mut_basicblock(op); bb_lam && from_outer_scope(bb_lam)) {
            w.DLOG("found BB from enclosing scope {}", op);
            return refine(w.call(attr::freeBB, op));
        }
        if (isa_cont(app, arg, i)) {
            if (match<attr>(attr::ret, op) || isa_retvar(op)) {
                return app;
            } else if (auto contlam = op->isa_mut<Lam>()) {
                return refine(w.call(attr::ret, contlam));
            } else {
                auto wrapper = eta_wrap(op, attr::ret)->set("eta_cont");
                w.DLOG("eta expanded return cont: {} -> {}", op, wrapper);
                return refine(wrapper);
            }
        }

        if (!isa_callee_br(app, arg, i)) {
            if (auto bb_lam = Lam::isa_mut_basicblock(op)) {
                w.DLOG("found firstclass use of BB: {}", bb_lam);
                return refine(w.call(attr::fstclassBB, bb_lam));
            }
            // TODO: If EtaRed eta-reduces branches, we have to wrap them again!
            if (isa_retvar(op)) {
                w.DLOG("found firstclass use of return var: {}", op);
                return refine(eta_wrap(op, attr::fstclassBB)->set("fstclass_ret"));
            }
        }
    }

    return app;
}

const App* ClosConvPrep::rewrite_callee(const App* app) {
    auto& w = world();
    if (Pi::isa_cn(app->callee_type())) {
        if (auto br = app->callee()->isa<Extract>()) {
            auto branches = br->tuple();
            // Eta-Expand branches
            if (branches->isa<Tuple>() && branches->type()->isa<Arr>()) {
                for (size_t i = 0, e = branches->num_ops(); i != e; ++i) {
                    if (!branches->op(i)->isa_mut<Lam>()) {
                        auto wrapper = eta_wrap(branches->op(i), attr::bot)->set("eta_br");
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

Ref ClosConvPrep::rewrite(Ref def) {
    if (ignore_ || match<attr>(def)) return def;

    if (auto app = def->isa<App>()) {
        app = rewrite_arg(app);
        app = rewrite_callee(app);
        return app;
    }

    return def;
}

} // namespace thorin::clos
