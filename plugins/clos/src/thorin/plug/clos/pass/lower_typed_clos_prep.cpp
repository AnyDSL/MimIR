#include "thorin/plug/clos/pass/lower_typed_clos_prep.h"

#include <thorin/plug/mem/mem.h>

namespace thorin::plug::clos {

namespace {

bool interesting_type(Ref type, DefSet& visited) {
    if (type->isa_mut()) visited.insert(type);
    if (isa_clos_type(type)) return true;
    if (auto sigma = type->isa<Sigma>())
        return std::any_of(sigma->ops().begin(), sigma->ops().end(),
                           [&](auto d) { return !visited.contains(d) && interesting_type(d, visited); });
    if (auto arr = type->isa<Arr>()) return interesting_type(arr->body(), visited);
    return false;
}

bool interesting_type(Ref def) {
    auto visited = DefSet();
    return interesting_type(def->type(), visited);
}

void split(DefSet& out, Ref def, bool as_callee) {
    if (auto lam = def->isa<Lam>()) {
        out.insert(lam);
    } else if (auto [var, lam] = ca_isa_var<Lam>(def); var && lam) {
        if (var->type()->isa<Pi>() || interesting_type(var)) out.insert(var);
    } else if (auto c = isa_clos_lit(def, false)) {
        split(out, c.fnc(), as_callee);
    } else if (auto a = match<attr>(def)) {
        split(out, a->arg(), as_callee);
    } else if (auto proj = def->isa<Extract>()) {
        split(out, proj->tuple(), as_callee);
    } else if (auto pack = def->isa<Pack>()) {
        split(out, pack->body(), as_callee);
    } else if (auto tuple = def->isa<Tuple>()) {
        for (auto op : tuple->ops()) split(out, op, as_callee);
    } else if (as_callee) {
        out.insert(def);
    }
}

DefSet split(Ref def, bool keep_others) {
    DefSet out;
    split(out, def, keep_others);
    return out;
}

} // namespace

undo_t LowerTypedClosPrep::set_esc(Ref def) {
    auto undo = No_Undo;
    for (auto d : split(def, false)) {
        if (is_esc(d)) continue;
        if (auto lam = d->isa_mut<Lam>())
            undo = std::min(undo, undo_visit(lam));
        else if (auto [var, lam] = ca_isa_var<Lam>(d); var && lam)
            undo = std::min(undo, undo_visit(lam));
        world().DLOG("set esc: {}", d);
        esc_.emplace(d);
    }
    return undo;
}

Ref LowerTypedClosPrep::rewrite(Ref def) {
    if (auto closure = isa_clos_lit(def, false)) {
        auto fnc = closure.fnc();
        if (!match<attr>(fnc)) {
            auto new_fnc = world().call(esc_.contains(fnc) ? attr::esc : attr::bot, fnc);
            return clos_pack(closure.env(), new_fnc, closure->type());
        }
    }
    return def;
}

undo_t LowerTypedClosPrep::analyze(Ref def) {
    auto& w = world();
    if (auto c = isa_clos_lit(def, false)) {
        w.DLOG("closure ({}, {})", c.env(), c.fnc());
        if (!c.fnc_as_lam() || is_esc(c.fnc_as_lam()) || is_esc(c.env_var())) return set_esc(c.env());
    } else if (auto store = match<mem::store>(def)) {
        w.DLOG("store {}", store->arg(2));
        return set_esc(store->arg(2));
    } else if (auto app = def->isa<App>(); app && Pi::isa_cn(app->callee_type())) {
        w.DLOG("app {}", def);
        auto undo    = No_Undo;
        auto callees = split(app->callee(), true);
        for (auto i = 0_u64; i < app->num_args(); i++) {
            if (!interesting_type(app->arg(i))) continue;
            if (std::any_of(callees.begin(), callees.end(), [&](Ref callee) {
                    if (auto lam = callee->isa_mut<Lam>()) return is_esc(lam->var(i));
                    return true;
                }))
                undo = std::min(undo, set_esc(app->arg(i)));
        }
        return undo;
    }
    return No_Undo;
}

} // namespace thorin::plug::clos
