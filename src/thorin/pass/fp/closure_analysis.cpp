#include "thorin/pass/fp/closure_analysis.h"

namespace thorin {

static bool interesting_type_b(const Def* type) {
    if (auto pi = type->isa<Pi>(); pi && pi->is_cn())
        return true;
    return isa_ctype(type) != nullptr;
}

static bool interesting_type(const Def* type, DefSet& visited) {
    if (type->isa_nom())
        visited.insert(type);
    if (interesting_type_b(type))
        return true;
    if (auto sigma = type->isa<Sigma>())
        return std::any_of(sigma->ops().begin(), sigma->ops().end(), [&](auto d) {
            return !visited.contains(d) && interesting_type(d, visited); });
    if (auto arr = type->isa<Arr>())
        return interesting_type(arr->body(), visited);
    return false;
}

static bool interesting_type(const Def* type) {
   auto v = DefSet();
   return interesting_type(type, v);
}

static void split(DefSet& out, const Def* def, bool keep_others) {
    if (auto lam = def->isa<Lam>())
        out.insert(lam);
    else if (auto [var, lam] = isa_lam_var(def); var && lam)
        out.insert(var);
    else if (auto c = isa_closure_lit(def))
        split(out, c.fnc(), keep_others);
    else if (auto [inner, _] = isa_mark(def); inner)
        split(out, inner, keep_others);
    else if (auto proj = def->isa<Extract>())
        split(out, proj->tuple(), keep_others);
    else if (auto pack = def->isa<Pack>())
        split(out, pack->body(), keep_others);
    else if (auto tuple = def->isa<Tuple>())
        for (auto op: tuple->ops())
            split(out, op, keep_others);
    else if (keep_others)
        out.insert(def);
}

static DefSet&& split(const Def* def, bool keep_others, DefSet&& out = DefSet()) {
    split(out, def, keep_others);
    return std::move(out);
}

ClosureAnalysis::Err ClosureAnalysis::error(const Def* def) {
    if (auto lam = def->isa_nom<Lam>())
        return {lam, undo_visit(lam)};
    if (auto [var, lam] = isa_lam_var(def); var && lam)
        return error(lam);
    assert(false && "only track variables and lams");
}


ClosureAnalysis::Err ClosureAnalysis::assign(const DefSet& defs, CA v) {
    auto e = ok();
    if (v == CA::bot)
        return e;
    for (auto def: defs) {
        auto prev = lookup(def);
        auto new_v = prev & v;
        if (new_v == prev)
            continue;
        if (prev != CA::bot) {
            e = std::min(e, error(def));
            world().DLOG("{}: {} -> {}", def, op2str(prev), op2str(new_v));
        } else {
            world().DLOG("init {}: {}", def, op2str(new_v));
        }
        switch (new_v) {
            case CA::proc_e:
                esc_proc_.insert(def);
                break;
            case CA::unknown:
                unknown_.insert(def);
                break;
            default: {
                data(def) = new_v;
                // If initialize a lam, we also initialize its variables
                // This ensures that we can make an "educated guess" at unknown, non-evil callsites
                auto lam = def->isa_nom<Lam>();
                if (!lam)
                    break;
                for (size_t i = 0; i < lam->num_vars(); i++) {
                    auto var = lam->var(i);
                    if (!interesting_type(var->type()))
                        continue;
                    auto var_v = (new_v == CA::jmp) ? CA::proc
                               : (new_v == CA::ret) ? CA::proc_e
                               : (var == lam->ret_var()) ? CA::ret : CA::proc;
                    e = std::min(e, assign({var}, var_v));
                }
            }
        }
    }
    return e;
}

// This is like simpl_lookup, but pre-assignes a lattice value if we have not seen 
// 'def' before. This is required if 'def' appears for the first time in (known) callee position.
// Note that 'RET' can never appear as a known callee
CA ClosureAnalysis::lookup_init(const Def* def) {
    CA v = lookup(def);
    if (v != CA::bot)
        return v;
    if (auto lam = def->isa_nom<Lam>()) {
        v = (!lam->is_set() || lam->ret_var()) ? CA::proc : CA::jmp;
    } else if (auto [var, lam] = isa_lam_var(def); var && lam) {
        v = (lam->ret_var() == var) ? CA::ret : CA::proc;
    } else {
        v = CA::unknown;
    }
    assign({def}, v);
    return v;
}

const Def* ClosureAnalysis::mark(const Def* def, CA a) {
    auto& w = world();
    auto [var, _] = isa_lam_var(def);
    if (auto tuple = def->isa<Tuple>(); tuple && !isa_closure_lit(def)) {
        auto new_ops = DefArray(tuple->num_ops(), [&](auto i) { return mark(tuple->op(i), a); });
        return w.tuple(tuple->type(), new_ops, tuple->dbg());
    } else if (auto arr = def->isa<Arr>()) {
        return arr->refine(1, mark(arr->body(), a));
    } else if (auto proj = def->isa<Extract>(); proj && !var) {
        return w.extract(mark(proj->tuple(), a),proj->index(), proj->dbg());
    } else if (auto [inner, b] = isa_mark(def); inner) {
        return (a == b) ? def : mark(inner, a);
    } else {
        // Lambda, Variable, Closure
        a = (a == CA::bot) ? lookup_init(def) : a;
        return w.ca_mark(def, a);
    }
}

void ClosureAnalysis::enter() {
    world().DLOG("enter {}", curr_nom());
    // Make sure we initialize the var's of global defs properly
    auto cur_lam = curr_nom()->as<Lam>();
    (void) lookup_init(cur_lam);
    // Compute free variables before any rewrites have been made
    // this should be true for any def reachable from curr_nom
    if (fva_) {
        fva_->run(cur_lam);
        analyze_captured_ = true;
    }
}

const Def* ClosureAnalysis::rewrite(const Def* def) {
    auto& w = world();

    if (auto app = def->isa<App>(); app && app->callee_type()->is_cn()) {
        w.DLOG("analyze/rw app: {}", def);
        auto num_args = app->num_args();
        auto absytpe = Array<CA>(app->callee_type()->num_doms(), CA::bot);
        for (auto c: split(app->callee(), true)) {
            for (size_t i = 0; i < num_args; i++) {
                if (!interesting_type(app->callee_type()->dom(i)))
                    continue;
                if (auto lam = c->isa_nom<Lam>()) {
                    absytpe[i] &= lookup_init(lam->var(i));
                } else if (auto [var, lam] = isa_lam_var(c); var && !is_evil(var)) {
                    auto v = lookup_init(var);
                    if ((v == CA::proc_e || v == CA::proc) && i == num_args - 1)
                        absytpe[i] &= CA::ret;
                    else
                        absytpe[i] &= CA::proc_e;
                } else {
                    absytpe[i] = CA::unknown;
                }
            }
        }

        auto err = ok();
        auto new_callee = mark(app->callee());
        auto new_args = app->args();
        for (size_t i = 0; i < app->num_args(); i++) {
            if (absytpe[i] != CA::bot) {
                auto arg = app->arg(i);
                err = std::min(err, assign(split(arg, false), absytpe[i]));
                new_args[i] = mark(arg);
            }
        }
        if (err)
            return proxy(def, err);
        return w.app(new_callee, new_args, app->dbg());

    } else if (auto closure = isa_closure_lit(def)) {
        w.DLOG("analyze/rw closure: {}", def);
        auto lam_v = CA::bot;
        auto env_v = CA::bot;
        for (auto f: split(closure.fnc(), false)) {
            auto lam = f->isa_nom<Lam>();
            assert(lam && "closure fnc = lam or folded branch");
            lam_v &= lookup_init(lam);
            env_v &= lookup_init(lam->var(CLOSURE_ENV_PARAM));
        }
        env_v &= (lam_v == CA::unknown || lam_v == CA::proc_e)
               ? CA::proc_e : CA::proc;

        if (auto err = assign(split(closure.env(), false), env_v)) {
            return proxy(def, err);
        } else {
            auto new_env = mark(closure.env(), env_v);
            return mark(pack_closure(new_env, closure.fnc(), closure.type()), lam_v);
        }

    } else if (auto store = isa<Tag::Store>(def)) {
        w.DLOG("analyze/rw store: {}", def);
        auto stored_defs = split(store->arg(), false);
        if (auto err = assign(stored_defs, CA::proc_e)) {
            return proxy(def, err);
        } else {
            auto args = store->args();
            args[2] = mark(args[2]);
            return w.app(store->callee(), args, store->dbg());
        }

    } 

    return def;
}

undo_t ClosureAnalysis::analyze(const Proxy* proxy) {
    auto [bad_exp, bad_lam] = proxy->ops<2>();
    world().DLOG("Undo {} in {}", bad_lam, bad_exp);
    return undo_visit(bad_lam->as_nom<Lam>());
}

undo_t ClosureAnalysis::analyze(const Def* def) {
    auto& w = world();
    auto err = ok();

    // Which *variables* are captured by escaping functions for which are closure has not yet been built?
    if (analyze_captured_ && !is_basicblock(curr_nom())) {
        auto v = is_escaping(curr_nom()) ? CA::proc_e : CA::proc;
        w.DLOG("analyze implicit captures of {} ({})", curr_nom(), op2str(v));
        auto& free_defs = fva_->run(curr_nom());
        for (auto def: free_defs) {
            if (auto [var, _] = isa_lam_var(def); var && interesting_type(var->type())) {
                w.DLOG("capture {}", var);
                err = std::min(err, assign({var}, v));
            }
        }
        analyze_captured_ = false;
    }

    // TODO Evilness

    return err.undo;
}

bool ClosureAnalysis::is_evil(const Def* def) {
    // TODO
    return false;
}

} // namespace thorin
