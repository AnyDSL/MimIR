#ifndef THORIN_CLOSURE_CONV_H
#define THORIN_CLOSURE_CONV_H

#include <functional>
#include <queue>
#include <vector>

#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

/// @brief Transitivly compute free @p Def's on demand.
/// This is used internally by @p ClosConv.
class FreeDefAna {
public:
    FreeDefAna(World& world)
        : world_(world)
        , cur_pass_id(1)
        , lam2nodes_(){};

    /// @brief run will compute free defs (FD) that appear in @p lam%s body.
    /// Nominal @p Def%s are only considered free if they are anottated with @p ClosKind::freeBB or @p
    /// ClosKind::fstclassBB. Otherwise, we add a nom's free defs in order to build a closure for it. Structural @p
    /// Def%s containing nominals are broken up if necessary.
    DefSet& run(Lam* lam);

private:
    /// @name analysis graph
    /// @{
    struct Node;
    using NodeQueue = std::queue<Node*>;
    using Nodes     = std::vector<Node*>;

    /// In order to save recomputing FDs sets, sets are computed for the subgraph reachable from nom and memorized.
    /// `pass_id` determines if the node has been initialized.
    struct Node {
        Def* nom;
        DefSet fvs;
        Nodes preds;
        Nodes succs;
        unsigned pass_id; //
    };
    /// @}

    /// @brief node is not initialized
    bool is_bot(Node* node) { return node->pass_id == 0; }

    /// @brief FD set for node is already present
    bool is_done(Node* node) { return !is_bot(node) && node->pass_id < cur_pass_id; }

    /// @brief mark node as done
    void mark(Node* node) { node->pass_id = cur_pass_id; }

    /// @brief split free def. This may create more Nodes as we discover reachable nodes.
    void split_fd(Node* node, const Def* fv, bool& is_init, NodeQueue& worklist);

    std::pair<Node*, bool> build_node(Def* nom, NodeQueue& worklist);
    void run(NodeQueue& worklist);

    World& world() { return world_; }

    World& world_;
    unsigned cur_pass_id;
    DefMap<std::unique_ptr<Node>> lam2nodes_;
};

/// @brief Perform *typed closure conversion*.
/// This is based on the [Simply Typed Closure Conversion](https://dl.acm.org/doi/abs/10.1145/237721.237791).
/// Closures are represented using dependen pairs `[env_type:*, cn[env_type, Args..], env_type]`.
/// In general only *continuations* are converted (i.e @p Lam%s that have type `cn[...]`).
/// Different kind of @p Lam%s may be rewritten differently:
/// - *returning* @p continuations, join-points and branches are fully closure converted
/// - *returning* continuations are not closure converted
/// - *first-class* continuations get a closures, but still have free variables.
///   There are hoisted into the *returning* @p Lam they belong to.
///
/// This pass relies on @p ClosConvPrep to introduce annotations for these cases.
///
/// Note: Since This direct-style @p Def%s are not rewritten, this can lead to problems with ceratin @p Axiom%s:
/// `ax : (B : *, int -> B) -> (int -> B)` won't be converted, possible arguments may.
/// Further, there is no machinery to handle free variables in a @p Lam%s type; this may lead to
/// problems with polymorphic functions.

class ClosConv {
public:
    ClosConv(World& world)
        : world_(world)
        , fva_(world)
        , closures_()
        , closure_types_()
        , worklist_(){};

    void run();

private:
    /// @name closure stub
    /// @{
    struct ClosureStub {
        Lam* old_fn;
        size_t num_fvs;
        const Def* env;
        Lam* fn;
    };

    ClosureStub make_stub(const DefSet& fvs, Lam* lam, Def2Def& subst);
    ClosureStub make_stub(Lam* lam, Def2Def& subst);
    /// @}

    /// @name recursively rewrite @p Def%s
    /// @{
    void rewrite_body(Lam* lam, Def2Def& subst);
    const Def* rewrite(const Def* old_def, Def2Def& subst);
    const Pi* rewrite_cont_type(const Pi*, Def2Def& subst);
    const Def* closure_type(const Pi* pi, Def2Def& subst, const Def* ent_type = nullptr);
    /// @}

    World& world() { return world_; }

    World& world_;
    FreeDefAna fva_;
    DefMap<ClosureStub> closures_;
    Def2Def closure_types_;
    std::queue<const Def*> worklist_;
};

// TODO: rename this
/// @brief Check is @p def is the variable of a @p nom of type @p N
template<class N>
std::tuple<const Extract*, N*> ca_isa_var(const Def* def) {
    if (auto proj = def->isa<Extract>()) {
        if (auto var = proj->tuple()->isa<Var>(); var && var->nom()->isa<N>())
            return std::tuple(proj, var->nom()->as<N>());
    }
    return {nullptr, nullptr};
}

/// @name closures
/// @{

/// @brief wrapper around a @p Def that can be used to match closures (see @p isa_clos_lit ).
class ClosLit {
public:
    /// @name Getters
    /// @{
    const Sigma* type() {
        assert(def_);
        return def_->type()->isa<Sigma>();
    }

    const Def* env();
    const Def* env_type() { return env()->type(); }

    const Def* fnc();
    const Pi* fnc_type() { return fnc()->type()->isa<Pi>(); }
    Lam* fnc_as_lam();

    const Def* env_var();
    const Def* ret_var() { return fnc_as_lam()->ret_var(); }
    ///@}

    operator bool() const { return def_ != nullptr; }

    operator const Tuple*() { return def_; }

    const Tuple* operator->() {
        assert(def_);
        return def_;
    }

    /// @name Properties
    /// @{
    bool is_returning() { return fnc_type()->is_returning(); }
    bool is_basicblock() { return fnc_type()->is_basicblock(); }
    ClosKind kind() { return kind_; } ///< @p ClosKind annoation. These should appear before the code-part.
    /// @}

private:
    ClosLit(const Tuple* def, ClosKind kind = ClosKind::bot)
        : def_(def)
        , kind_(kind){};

    const Tuple* def_;
    const ClosKind kind_;

    friend ClosLit isa_clos_lit(const Def*, bool);
};

/// @brief tries to match a closure literal
ClosLit isa_clos_lit(const Def* def, bool fn_isa_lam = true);

/// @brief pack a typed closure. This assumes that @p fn expects the environment as its @p CLOSURE_ENV_PARAM argument.
const Def* clos_pack_dbg(const Def* env, const Def* fn, const Def* dbg, const Def* ct = nullptr);

inline const Def* clos_pack(const Def* env, const Def* fn, const Def* ct = nullptr) {
    return clos_pack_dbg(env, fn, nullptr, ct);
}

/// @brief Deconstruct a closure into `(env_type, function, env)`
/// **Important**: use this or @p ClosLit to destruct closures, since typechecking for dependet pairs is currently
/// broken.
std::tuple<const Def*, const Def*, const Def*> clos_unpack(const Def* c);

/// @brief apply a closure to arguments
const Def* clos_apply(const Def* closure, const Def* args);
inline const Def* apply_closure(const Def* closure, Defs args) {
    auto& w = closure->world();
    return clos_apply(closure, w.tuple(args));
}

/// @}

/// @name closure types
/// @{

/// @brief return @p def if @p def is a closure and @c nullptr otherwise
const Sigma* isa_clos_type(const Def* def);

/// @brief creates a typed closure type from a @p Pi
Sigma* clos_type(const Pi* pi);

/// @brief Convert a closure type to a @p Pi, where the environment type has been removed or replaced by new_env_type
/// (if new_env_type != @c nullptr)
const Pi* clos_type_to_pi(const Def* ct, const Def* new_env_type = nullptr);

/// @}

std::tuple<Lam*, const Def*, const Def*> clos_lam_stub(const Def* env_type, const Def* dom, const Def* dbg = {});

/// @name closure environments
/// @p tup_or_sig should generally be a @p Tuple, @p Sigma or @p Var.
/// @{

/// Describes where the environment is placed in the argument list.
const size_t Clos_Env_Param = 1_u64;

const Def* clos_insert_env(size_t i, const Def* env, std::function<const Def*(size_t)> f);
inline const Def* clos_insert_env(size_t i, const Def* env, const Def* a) {
    return clos_insert_env(i, env, [&](auto i) { return a->proj(i); });
}

inline const Def* clos_insert_env(const Def* env, const Def* tup_or_sig) {
    auto& w      = tup_or_sig->world();
    auto new_ops = DefArray(tup_or_sig->num_projs() + 1, [&](auto i) { return clos_insert_env(i, env, tup_or_sig); });
    return (tup_or_sig->isa<Sigma>()) ? w.sigma(new_ops) : w.tuple(new_ops);
}

const Def* clos_remove_env(size_t i, std::function<const Def*(size_t)> f);
inline const Def* clos_remove_env(size_t i, const Def* def) {
    return clos_remove_env(i, [&](auto i) { return def->proj(i); });
}
inline const Def* clos_remove_env(const Def* tup_or_sig) {
    auto& w      = tup_or_sig->world();
    auto new_ops = DefArray(tup_or_sig->num_projs() - 1, [&](auto i) { return clos_remove_env(i, tup_or_sig); });
    return (tup_or_sig->isa<Sigma>()) ? w.sigma(new_ops) : w.tuple(new_ops);
}

inline const Def* clos_sub_env(const Def* tup_or_sig, const Def* new_env) {
    return tup_or_sig->refine(Clos_Env_Param, new_env);
}
/// @}

}; // namespace thorin

#endif
