#include "thorin/util/bitset.h"
#include "thorin/pass/fp/closure_analysis.h"

namespace thorin {

static bool interesting_type(const Def* type, DefSet& visited) {
    if (type->isa_nom())
        visited.insert(type);
    if (isa_ctype(type))
        return true;
    if (auto sigma = type->isa<Sigma>())
        return std::any_of(sigma->ops().begin(), sigma->ops().end(), [&](auto d) {
            return !visited.contains(d) && interesting_type(d, visited); });
    if (auto arr = type->isa<Arr>())
        return interesting_type(arr->body(), visited);
    return false;
}

static bool interesting_type(const Def* def) {
   auto visited = DefSet();
   return interesting_type(def->type(), visited);
}

static void split(DefSet& out, const Def* def, bool as_callee) {
    if (auto lam = def->isa<Lam>()) {
        out.insert(lam);
    } else if (auto [var, lam] = ca_isa_var<Lam>(def); var && lam) {
        if ((as_callee && var->type()->isa<Pi>()) || interesting_type(var))
            out.insert(var);
    } else if (auto c = isa_closure_lit(def)) {
        split(out, c.fnc_as_lam(), as_callee);
    } else if (auto q = isa<Tag::CA>(def)) {
        split(out, q->arg(), as_callee);
    } else if (auto proj = def->isa<Extract>()) {
        split(out, proj->tuple(), as_callee);
    } else if (auto pack = def->isa<Pack>()) {
        split(out, pack->body(), as_callee);
    } else if (auto tuple = def->isa<Tuple>()) {
        for (auto op: tuple->ops())
            split(out, op, as_callee);
    } else if (as_callee) {
        out.insert(def);
    }
}

static DefSet&& split(const Def* def, bool keep_others, DefSet&& out = DefSet()) {
    split(out, def, keep_others);
    return std::move(out);
}

undo_t ClosureAnalysis::set_escaping(const Def* def) {
    auto undo = No_Undo;
    for (auto d: split(def, false)) {
        if (is_escaping(d)) 
            continue;
        if (auto lam = d->isa_nom<Lam>())
            undo = std::min(undo, undo_visit(lam));
        else if (auto [var, lam] = ca_isa_var<Lam>(def); var && lam)
            undo = std::min(undo, undo_visit(lam));
        world().DLOG("set escaping: {}", d);
        escaping_.emplace(d);
    }
    return undo;
}

const Def* ClosureAnalysis::rewrite(const Def* def) {
    if (auto closure = isa_closure_lit(def, false)) {
        auto fnc = closure.fnc();
        if (!isa<Tag::CA>(fnc)) {
            auto new_fnc = world().ca_mark(fnc, escaping_.contains(fnc) ? CA::proc_e : CA::proc);
            return pack_closure(closure.env(), new_fnc, closure->type());
        }
    }
    return def;
}

undo_t ClosureAnalysis::analyze(const Def* def) {
    auto& w = world();
    if (auto c = isa_closure_lit(def, false)) {
        w.DLOG("closure ({}, {})", c.env(), c.fnc_as_lam());
        if (is_escaping(c.fnc_as_lam()) || is_escaping(c.env_var()))
            return set_escaping(c.env());
    } else if (auto store = isa<Tag::Store>(def)) {
        w.DLOG("store {}", store->arg());
        return set_escaping(store->arg());
    } else if (auto app = def->isa<App>(); app && app->callee_type()->is_cn()) {
        w.DLOG("app {}", def);
        auto undo = No_Undo;
        auto callees = split(app->callee(), true);
        for (auto i = 0_u64; i < app->num_args(); i++) {
            if (!interesting_type(app->arg(i))) 
                continue;
            if (std::any_of(callees.begin(), callees.end(), [&](const Def* callee) {
                if (auto lam = callee->isa_nom<Lam>()) return is_escaping(lam->var(i));
                return true;
            }))
                undo = std::min(undo, set_escaping(app->arg(i)));
        }
        return undo;
    }
    return No_Undo;
}

} // namespace thorin
