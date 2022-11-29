
#include "dialects/mem/passes/rw/reshape.h"

#include <functional>
#include <sstream>
#include <vector>

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

void Reshape::enter() { rewrite_def(curr_nom()); }

const Def* Reshape::rewrite_def(const Def* def) {
    if (auto i = old2new_.find(def); i != old2new_.end()) return i->second;
    auto new_def  = rewrite_def_(def);
    old2new_[def] = new_def;
    return new_def;
}

bool should_flatten(const Def* T) {
    // handle [] cases
    if (T->isa<Sigma>()) return true;
    // also handle normalized tuple-arrays ((a:I32,b:I32) : <<2;I32>>)
    if (auto lit = T->arity()->isa<Lit>()) { return lit->get<u64>() > 1; }
    return false;
}

const Def* Reshape::rewrite_def_(const Def* def) {
    // We ignore types.
    switch (def->node()) {
        // TODO: check if bot: Cn[[A,B],Cn[Ret]] is handled correctly
        // case Node::Bot:
        // case Node::Top:
        case Node::Type:
        case Node::Univ:
        case Node::Nat: return def;
    }

    // ignore axioms
    if (def->isa<Axiom>()) { return def; }

    // This is dead code for debugging purposes.
    // It allows for inspection of the current def.
    std::stringstream ss;
    ss << def << " : " << def->type() << " [" << def->node_name() << "]";
    std::string str = ss.str();

    // vars are handled by association.
    if (def->isa<Var>()) { world().ELOG("Var: {}", def); }
    assert(!def->isa<Var>());

    auto& w = world();

    if (auto app = def->isa<App>()) {
        auto callee = rewrite_def(app->callee());
        auto arg    = rewrite_def(app->arg());

        world().DLOG("callee: {} : {}", callee, callee->type());

        // Reshape normally (not to callee) to ensure that callee is reshaped correctly.
        auto reshaped_arg = reshape(arg);
        auto new_app      = w.app(callee, reshaped_arg);
        return new_app;
    } else if (auto lam = def->isa_nom<Lam>()) {
        world().DLOG("rewrite_def lam {} : {}", def, def->type());
        auto new_lam = reshape_lam(lam);
        world().DLOG("rewrote lam {} : {}", def, def->type());
        world().DLOG("into lam {} : {}", new_lam, new_lam->type());
        return new_lam;
    } else if (auto tuple = def->isa<Tuple>()) {
        DefArray elements(tuple->ops(), [&](const Def* op) { return rewrite_def(op); });
        return w.tuple(elements);
    } else {
        auto new_ops = DefArray(def->num_ops(), [&](auto i) { return rewrite_def(def->op(i)); });
        // Warning: if the new_type is not correct, inconcistencies will arise.
        auto new_type = rewrite_def(def->type());
        auto new_dbg  = def->dbg() ? rewrite_def(def->dbg()) : nullptr;

        auto new_def = def->rebuild(w, new_type, new_ops, new_dbg);
        return new_def;
    }
}

Lam* Reshape::reshape_lam(Lam* def) {
    auto pi_ty  = def->type();
    auto new_ty = reshape_type(pi_ty)->as<Pi>();

    auto& w       = def->world();
    Lam* new_lam  = w.nom_lam(new_ty, w.dbg(def->name() + "_reshaped"));
    old2new_[def] = new_lam;

    w.DLOG("Reshape lam: {} : {}", def, pi_ty);
    w.DLOG("         to: {} : {}", new_lam, new_ty);

    // We associate the arguments (reshape the old vars).
    // Alternatively, we could use beta reduction (reduce) to do this for us.
    auto new_arg = new_lam->var();

    // We deeply associate `def->var()` with `new_arg` in a reconstructed shape.
    // Idea: first make new_arg into "atomic" def list, then recrusively imitate `def->var`.
    auto reformed_new_arg = reshape(new_arg, def->var()->type()); // `def->var()->type() = pi_ty`
    w.DLOG("var {} : {}", def->var(), def->var()->type());
    w.DLOG("new var {} : {}", new_arg, new_arg->type());
    w.DLOG("reshaped new_var {} : {}", reformed_new_arg, reformed_new_arg->type());
    w.DLOG("{}", def->var()->type());
    w.DLOG("{}", reformed_new_arg->type());
    old2new_[def->var()] = reformed_new_arg;
    // TODO: add if necessary. This probably was an issue with unintended overriding due to bad previous naming.
    // TODO: Remove after testing.
    // old2new_[new_arg] = new_arg;

    auto new_body = rewrite_def(def->body());
    new_lam->set_body(new_body);
    new_lam->set_filter(true);

    if (def->is_external()) {
        new_lam->make_external();
        def->make_internal();
    }

    w.DLOG("finished transforming: {} : {}", new_lam, new_ty);
    return new_lam;
}

std::vector<const Def*> flatten_ty(const Def* T) {
    std::vector<const Def*> types;
    if (should_flatten(T)) {
        for (auto P : T->projs()) {
            auto inner_types = flatten_ty(P);
            types.insert(types.end(), inner_types.begin(), inner_types.end());
        }
    } else {
        types.push_back(T);
    }
    return types;
}

bool is_mem_ty(const Def* T) { return match<mem::M>(T); }
DefArray vec2array(const std::vector<const Def*>& vec) { return DefArray(vec.begin(), vec.end()); }

const Def* Reshape::reshape_type(const Def* T) {
    auto& w = T->world();

    if (auto pi = T->isa<Pi>()) {
        auto new_dom = reshape_type(pi->dom());
        auto new_cod = reshape_type(pi->codom());
        return w.pi(new_dom, new_cod);
    } else if (auto sigma = T->isa<Sigma>()) {
        auto flat_types = flatten_ty(sigma);
        std::vector<const Def*> new_types(flat_types.size());
        std::transform(flat_types.begin(), flat_types.end(), new_types.begin(),
                       [&](auto T) { return reshape_type(T); });
        if (mode_ == Mode::Flat) {
            auto reshaped_type = w.sigma(vec2array(new_types));
            return reshaped_type;
        } else {
            if (new_types.size() == 0) return w.sigma();
            if (new_types.size() == 1) return new_types[0];
            const Def* mem = nullptr;
            const Def* ret = nullptr;
            // find mem, erase all mems
            for (auto i = new_types.begin(); i != new_types.end(); i++) {
                if (is_mem_ty(*i)) {
                    if (!mem) mem = *i;
                    new_types.erase(i);
                }
            }
            // TODO: more fine-grained test
            if (new_types.back()->isa<Pi>()) {
                ret = new_types.back();
                new_types.pop_back();
            }
            // Create the arg form `[[mem,args],ret]`
            const Def* args = w.sigma(vec2array(new_types));
            if (mem) { args = w.sigma({mem, args}); }
            if (ret) { args = w.sigma({args, ret}); }
            return args;
        }
    } else {
        return T;
    }
}

std::vector<const Def*> flatten_def(const Def* def) {
    std::vector<const Def*> defs;
    if (should_flatten(def->type())) {
        auto& w = def->world();
        for (auto P : def->projs()) {
            auto inner_defs = flatten_def(P);
            defs.insert(defs.end(), inner_defs.begin(), inner_defs.end());
        }
    } else {
        defs.push_back(def);
    }
    return defs;
}

const Def* Reshape::reshape(std::vector<const Def*>& defs, const Def* T) {
    auto& world = T->world();
    if (should_flatten(T)) {
        DefArray tuples(T->projs(), [&](auto P) { return reshape(defs, P); });
        return world.tuple(tuples);
    } else {
        assert(defs.size() > 0 && "Reshape: not enough arguments");
        auto def = defs.front();
        defs.erase(defs.begin());
        // For inner function types, we override the type
        if (!def->type()->isa<Pi>()) {
            if (world.checker().equiv(def->type(), T, {})) {
                world.ELOG("reconstruct T {} from def {}", T, def->type());
            }
            assert(world.checker().equiv(def->type(), T, {}) && "Reshape: argument type mismatch");
        }
        return def;
    }
}

const Def* Reshape::reshape(const Def* def, const Def* target) {
    auto flat_defs = flatten_def(def);
    return reshape(flat_defs, target);
}

// called for new lambda arguments, app arguments
// We can not (directly) replace it with the more general version above due to the mem erasure.
// TODO: ignore mem erase, replace with more general
// TODO: capture names
const Def* Reshape::reshape(const Def* def) {
    auto& w = def->world();

    auto flat_defs = flatten_def(def);
    if (flat_defs.size() == 1) return flat_defs[0];
    if (mode_ == Mode::Flat) {
        return w.tuple(vec2array(flat_defs));
    } else {
        // arg style
        // [[mem,args],ret]
        const Def* mem = nullptr;
        const Def* ret = nullptr;
        // find mem, erase all mems
        for (auto i = flat_defs.begin(); i != flat_defs.end(); i++) {
            if (is_mem_ty((*i)->type())) {
                if (!mem) mem = *i;
                flat_defs.erase(i);
            }
        }
        if (flat_defs.back()->type()->isa<Pi>()) {
            ret = flat_defs.back();
            flat_defs.pop_back();
        }
        const Def* args = w.tuple(vec2array(flat_defs));
        if (mem) { args = w.tuple({mem, args}); }
        if (ret) { args = w.tuple({args, ret}); }
        return args;
    }
}

} // namespace thorin::mem
