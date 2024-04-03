#include "thorin/plug/mem/pass/reshape.h"

#include <functional>
#include <sstream>
#include <vector>

#include <thorin/check.h>
#include <thorin/def.h>
#include <thorin/lam.h>
#include <thorin/tuple.h>

#include "thorin/plug/mem/mem.h"

namespace thorin::plug::mem {

namespace {

bool is_mem_ty(const Def* T) { return match<mem::M>(T); }

// TODO merge with should_flatten from tuple.*
bool should_flatten(const Def* T) {
    // handle [] cases
    if (T->isa<Sigma>()) return true;
    // also handle normalized tuple-arrays ((a:I32,b:I32) : <<2;I32>>)
    // TODO: handle better than with magic number
    //  (do we want to flatten any array with more than 2 elements?)
    //  (2 elements are needed for conditionals)
    // TODO: e.g. lea explicitely does not want to flatten

    // TODO: annotate with test cases that need these special cases
    // Problem with 2 Arr -> flatten
    // lea (2, <<2;I32>>, ...) -> lea (2, I32, I32, ...)
    if (auto lit = T->arity()->isa<Lit>(); lit && lit->get<u64>() <= 2) {
        if (auto arr = T->isa<Arr>(); arr && arr->body()->isa<Pi>()) return lit->get<u64>() > 1;
    }
    return false;
}

// TODO merge with tuple.*
DefVec flatten_ty(const Def* T) {
    DefVec types;
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

// TODO try to remove code duplication with flatten_ty
DefVec flatten_def(const Def* def) {
    DefVec defs;
    if (should_flatten(def->type())) {
        for (auto P : def->projs()) {
            auto inner_defs = flatten_def(P);
            defs.insert(defs.end(), inner_defs.begin(), inner_defs.end());
        }
    } else {
        defs.push_back(def);
    }
    return defs;
}

} // namespace

void Reshape::enter() { rewrite_def(curr_mut()); }

const Def* Reshape::rewrite_def(const Def* def) {
    if (auto i = old2new_.find(def); i != old2new_.end()) return i->second;
    auto new_def  = rewrite_def_(def);
    old2new_[def] = new_def;
    return new_def;
}

const Def* Reshape::rewrite_def_(const Def* def) {
    // We ignore types, Globals, and Axioms.
    switch (def->node()) {
        // TODO: check if bot: Cn[[A,B],Cn[Ret]] is handled correctly
        // case Node::Bot:
        // case Node::Top:
        case Node::Type:
        case Node::Univ:
        case Node::Nat:
        case Node::Axiom:
        case Node::Global: return def;
    }

    // This is dead code for debugging purposes.
    // It allows for inspection of the current def.
    std::stringstream ss;
    ss << def << " : " << def->type() << " [" << def->node_name() << "]";
    std::string str = ss.str();

    // vars are handled by association.
    if (def->isa<Var>()) world().ELOG("Var: {}", def);
    assert(!def->isa<Var>());

    if (auto app = def->isa<App>()) {
        auto callee = rewrite_def(app->callee());
        auto arg    = rewrite_def(app->arg());

        world().DLOG("callee: {} : {}", callee, callee->type());

        // Reshape normally (not to callee) to ensure that callee is reshaped correctly.
        auto reshaped_arg = reshape(arg);
        world().DLOG("reshape arg {} : {}", arg, arg->type());
        world().DLOG("into arg {} : {}", reshaped_arg, reshaped_arg->type());
        auto new_app = world().app(callee, reshaped_arg);
        return new_app;
    } else if (auto lam = def->isa_mut<Lam>()) {
        world().DLOG("rewrite_def lam {} : {}", def, def->type());
        auto new_lam = reshape_lam(lam);
        world().DLOG("rewrote lam {} : {}", def, def->type());
        world().DLOG("into lam {} : {}", new_lam, new_lam->type());
        return new_lam;
    } else if (auto tuple = def->isa<Tuple>()) {
        auto elements = DefVec(tuple->ops(), [&](const Def* op) { return rewrite_def(op); });
        return world().tuple(elements);
    } else {
        auto new_ops = DefVec(def->num_ops(), [&](auto i) { return rewrite_def(def->op(i)); });
        // Warning: if the new_type is not correct, inconcistencies will arise.
        auto new_type = rewrite_def(def->type());
        auto new_def  = def->rebuild(new_type, new_ops);
        return new_def;
    }
}

Lam* Reshape::reshape_lam(Lam* old_lam) {
    if (!old_lam->is_set()) {
        world().DLOG("reshape_lam: {} is not a set", old_lam);
        return old_lam;
    }
    auto pi_ty  = old_lam->type();
    auto new_ty = reshape_type(pi_ty)->as<Pi>();

    Lam* new_lam;
    if (*old_lam->sym() == "main") {
        new_lam = old_lam;
    } else {
        new_lam = old_lam->stub(new_ty);
        if (!old_lam->is_external()) new_lam->debug_suffix("_reshape");
        old2new_[old_lam] = new_lam;
    }

    world().DLOG("Reshape lam: {} : {}", old_lam, pi_ty);
    world().DLOG("         to: {} : {}", new_lam, new_ty);

    // We associate the arguments (reshape the old vars).
    // Alternatively, we could use beta reduction (reduce) to do this for us.
    auto new_arg = new_lam->var();

    // We deeply associate `old_lam->var()` with `new_arg` in a reconstructed shape.
    // Idea: first make new_arg into "atomic" old_lam list, then recrusively imitate `old_lam->var`.
    auto reformed_new_arg = reshape(new_arg, old_lam->var()->type()); // `old_lam->var()->type() = pi_ty`
    world().DLOG("var {} : {}", old_lam->var(), old_lam->var()->type());
    world().DLOG("new var {} : {}", new_arg, new_arg->type());
    world().DLOG("reshaped new_var {} : {}", reformed_new_arg, reformed_new_arg->type());
    world().DLOG("{}", old_lam->var()->type());
    world().DLOG("{}", reformed_new_arg->type());
    old2new_[old_lam->var()] = reformed_new_arg;
    // TODO: add if necessary. This probably was an issue with unintended overriding due to bad previous naming.
    // TODO: Remove after testing.
    // old2new_[new_arg] = new_arg;

    auto new_body   = rewrite_def(old_lam->body());
    auto new_filter = rewrite_def(old_lam->filter());
    new_lam->unset();
    new_lam->set(new_filter, new_body);

    if (old_lam->is_external()) old_lam->transfer_external(new_lam);

    world().DLOG("finished transforming: {} : {}", new_lam, new_ty);
    return new_lam;
}

const Def* Reshape::reshape_type(const Def* T) {
    if (auto pi = T->isa<Pi>()) {
        auto new_dom = reshape_type(pi->dom());
        auto new_cod = reshape_type(pi->codom());
        return world().pi(new_dom, new_cod);
    } else if (auto sigma = T->isa<Sigma>()) {
        auto flat_types = flatten_ty(sigma);
        auto new_types  = DefVec(flat_types.size());
        std::ranges::transform(flat_types, new_types.begin(), [&](auto T) { return reshape_type(T); });
        if (mode_ == Mode::Flat) {
            const Def* mem = nullptr;
            // find mem
            for (auto i = new_types.begin(); i != new_types.end(); i++)
                if (is_mem_ty(*i) && !mem) mem = *i;
            // filter out mems
            new_types.erase(std::remove_if(new_types.begin(), new_types.end(), is_mem_ty), new_types.end());
            // readd mem in the front
            if (mem) new_types.insert(new_types.begin(), mem);
            auto reshaped_type = world().sigma(new_types);
            return reshaped_type;
        } else {
            if (new_types.size() == 0) return world().sigma();
            if (new_types.size() == 1) return new_types[0];
            const Def* mem = nullptr;
            const Def* ret = nullptr;
            // find mem
            for (auto i = new_types.begin(); i != new_types.end(); i++)
                if (is_mem_ty(*i) && !mem) mem = *i;
            // filter out mems
            new_types.erase(std::remove_if(new_types.begin(), new_types.end(), is_mem_ty), new_types.end());
            // TODO: more fine-grained test
            if (new_types.back()->isa<Pi>()) {
                ret = new_types.back();
                new_types.pop_back();
            }
            // Create the arg form `[[mem,args],ret]`
            const Def* args = world().sigma(new_types);
            if (mem) args = world().sigma({mem, args});
            if (ret) args = world().sigma({args, ret});
            return args;
        }
    } else {
        return T;
    }
}

const Def* Reshape::reshape(DefVec& defs, const Def* T, const Def* mem) {
    auto& world = T->world();
    if (should_flatten(T)) {
        auto tuples = T->projs([&](auto P) { return reshape(defs, P, mem); });
        return world.tuple(tuples);
    } else {
        const Def* def;
        if (is_mem_ty(T)) {
            assert(mem != nullptr && "Reshape: mems not found");
            def = mem;
        } else {
            do {
                assert(defs.size() > 0 && "Reshape: not enough arguments");
                def = defs.front();
                defs.erase(defs.begin());
            } while (is_mem_ty(def->type()));
        }
        // For inner function types, we override the type
        if (!def->type()->isa<Pi>()) {
            if (!Check::alpha(def->type(), T)) world.ELOG("reconstruct T {} from def {}", T, def->type());
            assert(Check::alpha(def->type(), T) && "Reshape: argument type mismatch");
        }
        return def;
    }
}

const Def* Reshape::reshape(const Def* def, const Def* target) {
    def->world().DLOG("reshape:\n  {} =>\n  {}", def->type(), target);
    auto flat_defs = flatten_def(def);
    const Def* mem = nullptr;
    // find mem
    for (auto i = flat_defs.begin(); i != flat_defs.end(); i++)
        if (is_mem_ty((*i)->type()) && !mem) mem = *i;
    def->world().DLOG("mem: {}", mem);
    return reshape(flat_defs, target, mem);
}

// called for new lambda arguments, app arguments
// We can not (directly) replace it with the more general version above due to the mem erasure.
// TODO: ignore mem erase, replace with more general
// TODO: capture names
const Def* Reshape::reshape(const Def* def) {
    auto flat_defs = flatten_def(def);
    if (flat_defs.size() == 1) return flat_defs[0];
    // TODO: move mem removal to flatten_def
    if (mode_ == Mode::Flat) {
        const Def* mem = nullptr;
        // find mem
        for (auto i = flat_defs.begin(); i != flat_defs.end(); i++)
            if (is_mem_ty((*i)->type()) && !mem) mem = *i;
        // filter out mems
        flat_defs.erase(
            std::remove_if(flat_defs.begin(), flat_defs.end(), [](const Def* def) { return is_mem_ty(def->type()); }),
            flat_defs.end());
        // insert mem
        if (mem) flat_defs.insert(flat_defs.begin(), mem);
        return world().tuple(flat_defs);
    } else {
        // arg style
        // [[mem,args],ret]
        const Def* mem = nullptr;
        const Def* ret = nullptr;
        // find mem
        for (auto i = flat_defs.begin(); i != flat_defs.end(); i++)
            if (is_mem_ty((*i)->type()) && !mem) mem = *i;
        // filter out mems
        flat_defs.erase(
            std::remove_if(flat_defs.begin(), flat_defs.end(), [](const Def* def) { return is_mem_ty(def->type()); }),
            flat_defs.end());
        if (flat_defs.back()->type()->isa<Pi>()) {
            ret = flat_defs.back();
            flat_defs.pop_back();
        }
        const Def* args = world().tuple(flat_defs);
        if (mem) args = world().tuple({mem, args});
        if (ret) args = world().tuple({args, ret});
        return args;
    }
}

} // namespace thorin::plug::mem
