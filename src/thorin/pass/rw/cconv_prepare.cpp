
#include "thorin/pass/rw/cconv_prepare.h"
#include "thorin/pass/fp/eta_exp.h"

#include "thorin/transform/closure_conv.h"

namespace thorin {

// FIXME: these guys do not work if another pass rewrites curr_nom()'s body
static bool isa_cont(const App* body, const Def* def, size_t i) {
    return body->callee_type()->is_returning() && body->arg() == def && i == def->num_ops() - 1;
}

static const Def* isa_br(const App* body, const Def* def, size_t i) {
    if (!body->callee_type()->is_cn()) return nullptr;
    auto proj = body->callee()->isa<Extract>();
    return (proj
        && proj->tuple() == def
        && proj->tuple()->isa<Tuple>()) ? proj->tuple() : nullptr;
}

static bool isa_callee_br(const App* body, const Def* def, size_t i) {
    if (!body->callee_type()->is_cn())
        return false;
    return isa_callee(def, i) || isa_br(body, def, i);
}

static Lam* isa_retvar(const Def* def) {
    if (auto [var, lam] = ca_isa_var<Lam>(def); var && lam && var == lam->ret_var())
        return lam;
    return nullptr;
}

Lam* CConvPrepare::scope(Lam* lam) { 
    if (eta_exp_)
        lam = eta_exp_->new2old(lam);
    return lam2fscope_[lam];
}

void CConvPrepare::enter() {
    if (curr_nom()->type()->is_returning()) {
        lam2fscope_[curr_nom()] = curr_nom();
        world().DLOG("scope {} -> {}", curr_nom(), curr_nom());
        auto scope = Scope(curr_nom());
        for (auto def: scope.bound()) {
            assert(def);
            if (auto bb_lam = def->isa_nom<Lam>(); bb_lam && bb_lam->is_basicblock()) {
                world().DLOG("scope {} -> {}", bb_lam, curr_nom());
                lam2fscope_[bb_lam] = curr_nom();
            }
        }
    }
    if (auto body = curr_nom()->body()->isa<App>(); !wrapper_.contains(curr_nom()) && body && body->callee_type()->is_cn())
        cur_body_ = body;
    else
        cur_body_ = nullptr;
}

const Def* CConvPrepare::rewrite(const Def* def) {
    auto& w = world();
    if (!cur_body_ || isa<Tag::CConv>(def) || def->isa<Var>()) return def;
    for (auto i = 0u; i < def->num_ops(); i++) {
        auto op = def->op(i);
        auto refine = [&](const Def* new_op) {
            auto new_def = def->refine(i, new_op);
            if (def == cur_body_->callee())
                cur_body_ = cur_body_->refine(0, new_def)->as<App>();
            else if (def == cur_body_->arg())
                cur_body_ = cur_body_->refine(1, new_def)->as<App>();
            else if (isa_br(cur_body_, def, i))
                cur_body_ = cur_body_->refine(0, cur_body_->callee()->refine(0, new_def))->as<App>();
            return new_def;
        };
        if (auto lam = isa_retvar(op); lam && from_outer_scope(lam)) {
            w.DLOG("found return var from enclosing scope: {}", op);
            return refine(eta_wrap(op, CConv::freeBB, "free_ret"));
        }
        if (auto bb_lam = op->isa_nom<Lam>(); bb_lam && bb_lam->is_basicblock() && from_outer_scope(bb_lam)) {
            w.DLOG("found BB from enclosing scope {}", op);
            return refine(w.cconv_mark(op, CConv::freeBB));
        }
        if (isa_cont(cur_body_, def, i)) {
            if (isa<Tag::CConv>(CConv::ret, op) || isa_retvar(op)) {
                return def;
            } else if (auto contlam = op->isa_nom<Lam>()) {
                return refine(w.cconv_mark(contlam, CConv::ret));
            } else {
                auto wrapper = eta_wrap(op, CConv::ret, "eta_cont");
                w.DLOG("eta expanded return cont: {} -> {}", op, wrapper);
                return refine(wrapper);
            }
        }
        if (auto bb_lam = op->isa_nom<Lam>(); bb_lam && bb_lam->is_basicblock() && !isa_callee_br(cur_body_, def, i)) {
            w.DLOG("found firstclass use of BB: {}", bb_lam);
            return refine(w.cconv_mark(bb_lam, CConv::fstclassBB));
        }
        // TODO: If EtaRed eta-reduces branches, we have to wrap them again!
        if (isa_retvar(op) && !isa_callee_br(cur_body_, def, i)) {
            w.DLOG("found firstclass use of return var: {}", op);
            return refine(eta_wrap(op, CConv::fstclassBB, "fstclass_ret"));
        }
    }

    // Eta-Expand branches
    if (auto app = def->isa<App>(); app && app->callee_type()->is_cn()) {
        auto br = app->callee()->isa<Extract>();
        if (!br) return def;
        auto branches = br->tuple();
        for (auto i = 0u; i < branches->num_ops(); i++) {
            if (!branches->op(i)->isa_nom<Lam>()) {
                auto wrapper = eta_wrap(branches->op(i), CConv::bot, "eta_br");
                w.DLOG("eta wrap branch: {} -> {}", branches->op(i), wrapper);
                branches = branches->refine(i, wrapper);
            }
        }
        cur_body_ = app->refine(0, app->callee()->refine(0, branches))->as<App>();
        return cur_body_;
    }

    return def;
}

} // namespace thorin
