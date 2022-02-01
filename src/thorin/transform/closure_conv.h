#ifndef THORIN_CLOSURE_CONV_H
#define THORIN_CLOSURE_CONV_H

#include <functional>
#include <queue>
#include <vector>

#include "thorin/world.h"
#include "thorin/analyses/scope.h"

namespace thorin {

/// @brief Perform free variable analyses.
class FVA {
public:
    FVA(World& world)
        : world_(world)
        , cur_pass_id(1)
        , lam2nodes_() {};

    /// @p run will compute free defs that appear transitively in @p lam%s body.
    /// Nominal @p Def%s are never considered free (but their free variables are).
    /// Structural @p Def%s containing nominals are broken up.
    /// The results are memorized.
    DefSet& run(Lam *lam);

private:
    struct Node;
    using NodeQueue = std::queue<Node*>;
    using Nodes = std::vector<Node*>;

    struct Node {
        Def *nom;
        DefSet fvs;
        Nodes preds;
        Nodes succs;
        unsigned pass_id;
    };

    bool is_bot(Node* node) { return node->pass_id == 0; }
    bool is_done(Node* node) {
        return !is_bot(node) && node->pass_id < cur_pass_id;
    }
    void mark(Node* node) { node->pass_id = cur_pass_id; }

    void split_fv(Def *nom, const Def* fv, DefSet& out);

    std::pair<Node*, bool> build_node(Def* nom, NodeQueue& worklist);
    void run(NodeQueue& worklist);

    World& world() { return world_; }

    World& world_;
    unsigned cur_pass_id;
    DefMap<std::unique_ptr<Node>> lam2nodes_;
};


/// Perform typed closure conversion.
/// Closures are represented using existential types <code>[env_type:*, env : env_type, cn[env_type, Args..]]</code>
/// Only non-returning @p Lam%s are converted (i.e that have type cn[...])
/// This can lead to bugs in combinations with @p Axiom%s / @p Lam%s that are polymorphic in their arguments
/// return type:
/// <code>ax : ∀[B]. (int -> B) -> (int -> B)</code> won't be converted, possible arguments may.
/// Further, there is no machinery to handle free variables in a @p Lam%s type; this may lead to
/// problems with polymorphic functions.
/// Neither of this two cases is checked.
/// The types of @p Axiom%s are adjusted as well.

class ClosureConv {
public:

    enum Mode : uint8_t {
        MAX,  ///< closure convert functions, branches and return continuations
        SSI,  ///< closure convert functions and branches only
        MIN   ///< closure convert functions only
    };

    ClosureConv(World& world, Mode mode, bool keep_annots)
        : world_(world)
        , fva_(world)
        , closures_()
        , closure_types_()
        , worklist_() 
        , mode_(mode)
        , keep_annots_(keep_annots)
    {};

    void run();

private:
    struct ClosureStub {
        Lam* old_fn;
        size_t num_fvs;
        const Def* env;
        Lam* fn;
    };

    void rewrite_body(Lam* lam, Def2Def& subst);
    const Def* rewrite(const Def* old_def, Def2Def& subst, CA ca = CA::bot);
    const Def* rw_non_captured(const Def* var, Def2Def& subst, CA ca = CA::bot);
    const Def* closure_type(const Pi* pi, Def2Def& subst, const Def* ent_type = nullptr);
    ClosureStub make_stub(Lam* lam, Def2Def& subst, bool covert);
    const Def* make_closure(Lam* lam, Def2Def& subst, CA ca);

    bool convert_lam(CA ca) {
        if (ca == CA::ret)
            return mode_ >= MIN;
        if (ca == CA::br)
            return mode_ >= SSI;
        return true;
    }

    World& world() { return world_; }

    World& world_;
    FVA fva_;
    DefMap<ClosureStub> closures_;
    Def2Def closure_types_;
    std::queue<const Def*> worklist_;
    const Mode mode_;
    const bool keep_annots_;
};

/// # Auxillary functions to deal with closures #

/// Closure literal
/// Closure literals can have to forms:
/// - Lambdas: <code>(type, lambda, env)</code>
/// - Folded branches: <code>(type, proj i (lam_0, .., lam_N), proj i (env_0, .., env_N))</code>
/// The later form is introduced by the @p UnboxClosures pass.

/* marks */

// join 
CA operator&(CA a, CA b);
inline CA& operator&=(CA& a, const CA& b) {
    return a = a & b;
} 

inline bool ca_is_escaping(CA v) { return v == CA::unknown || v == CA::proc_e; }
inline bool ca_is_basicblock(CA v) { return v == CA::br || v == CA::ret; }
inline bool ca_is_proc(CA v) { return v == CA::proc || v == CA::proc_e; }

template<class N>
std::tuple<const Def*, N*> ca_isa_var(const Def* def) {
    if (auto proj = def->isa<Extract>()) {
        if (auto var = proj->tuple()->isa<Var>(); var && var->nom()->isa<N>())
            return std::tuple(proj, var->nom()->as<N>());
    }
    return {nullptr,  nullptr};
}

std::tuple<const Def*, CA> ca_isa_mark(const Def* def);
std::tuple<Lam*, CA> ca_isa_marked_lam(const Def* def);

class ClosureLit {
public:
    /// @name Getters
    /// @{
    const Sigma* type() {
        assert(def_);
        return def_->type()->isa<Sigma>();
    }

    const Def* env();
    const Def* env_type() {
        return env()->type();
    }

    const Def* fnc();
    const Pi* fnc_type() {
        return fnc()->type()->isa<Pi>();
    }
    Lam* fnc_as_lam();
    std::pair<const Def*, const Tuple*> fnc_as_folded();

    const Def* var(size_t i); ///< Get the @i%th variable of the function component
    const Def* env_var();
    const Def* ret_var() {
        return var(fnc_type()->num_doms() - 1);
    }
    ///@}

    operator bool() const {
        return def_ != nullptr;
    }

    operator const Tuple*() {
        return def_;
    }

    const Tuple* operator->() {
        assert(def_); 
        return def_;
    }

    /// @name Properties
    /// @{
    unsigned int order();
    bool is_escaping() { return ca_is_escaping(mark_); };
    bool is_basicblock() { return ca_is_basicblock(mark_); };
    bool is_proc() { return ca_is_proc(mark_); };
    bool is(CA a) { return mark_ == a; }
    CA mark() { return mark_; }
    /// @}

private:
    ClosureLit(const Tuple* def, CA mark = CA::bot)
        : def_(def), mark_(mark)
    {};

    const Tuple* def_;
    const CA mark_;

    friend ClosureLit isa_closure_lit(const Def*, bool);
};


/// return @p def if @p def is a closure and @c nullptr otherwise
const Sigma* isa_ctype(const Def* def);

/// creates a typed closure type from a @p Pi
Sigma* ctype(const Pi* pi);

/// Convert a closure type to a @p Pi, where the environment type has been removed
/// or replaced by new_env_type (if new_env_type != @c nullptr)
const Pi* ctype_to_pi(const Def* ct, const Def* new_env_type = nullptr);

/// tries to match a closure literal
ClosureLit isa_closure_lit(const Def* def, bool lambda_or_branch = true);

/// pack a typed closure. This assumes that @p fn expects the environment as its @p CLOSURE_ENV_PARAM argument.
const Def* pack_closure_dbg(const Def* env, const Def* fn, const Def* dbg, const Def* ct = nullptr);
inline const Def* pack_closure(const Def* env, const Def* fn, const Def* ct = nullptr) {
    return pack_closure_dbg(env, fn, nullptr, ct);
}

/// Deconstruct a closure into (env_type, function, env)
std::tuple<const Def*, const Def*, const Def*> unpack_closure(const Def* c);

/// Which param is the env param
const size_t CLOSURE_ENV_PARAM = 1_u64;

/// Return env at the env position and f(i')) otherwise where i' has been shifted
const Def* closure_insert_env(size_t i, const Def* env, std::function<const Def* (size_t)> f);
inline const Def* closure_insert_env(size_t i, const Def* env, const Def* a) {
    return closure_insert_env(i, env, [&](auto i) { return a->proj(i); });
}
inline const Def* closure_insert_env(const Def* env, const Def* def) {
    return def->rebuild(def->world(), def->type(), DefArray(def->num_ops() + 1, [&](auto i) {
        return closure_insert_env(i, env, def);
    }), def->dbg());

}

const Def* closure_remove_env(size_t i, std::function<const Def* (size_t)> f);
inline const Def* closure_remove_env(size_t i, const Def* def) {
    return closure_remove_env(i, [&](auto i) { return def->proj(i); });
}
inline const Def* closure_remove_env(const Def* def) {
    return def->rebuild(def->world(), def->type(), DefArray(def->num_ops() - 1, [&](auto i) { 
        return closure_remove_env(i, def);
    }), def->dbg());
}

const Def* apply_closure(const Def* closure, const Def* args);
template<typename T>
inline const Def* apply_closure(const Def* closure, T&& args) {
    auto& w = closure->world();
    return apply_closure(closure, w.tuple(std::forward<T>(args)));
}

};

#endif
