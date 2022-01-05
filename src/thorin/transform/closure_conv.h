#ifndef THORIN_CLOSURE_CONV_H
#define THORIN_CLOSURE_CONV_H

#include <queue>
#include <vector>
#include <set>

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
        ClosureConv(World& world)
            : world_(world)
            , fva_(world)
            , closures_(DefMap<Closure>())
            , closure_types_(Def2Def())
            , worklist_(std::queue<const Def*>()) {};

        void run();

    private:
        struct Closure {
            Lam* old_fn;
            size_t num_fvs;
            const Def* env;
            Lam* fn;
        };

        const Def* rewrite(const Def* old_def, Def2Def& subst);

        const Def* closure_type(const Pi* pi, Def2Def& subst, const Def* ent_type = nullptr);

        Closure make_closure(Lam* lam, Def2Def& subst);

        World& world() { return world_; }

        World& world_;
        FVA fva_;
        DefMap<Closure> closures_;
        Def2Def closure_types_;
        std::queue<const Def*> worklist_;
};

/// # Auxillary functions to deal with closures #

/// Closure literal
/// This comes in two flavors:
/// - @p TYPED, using existentials to hide the type of the environment
/// - @p UNTYPED, using pointers and <code>:bitcast</code>s to hide the environment
/// @ClosureConv introduces @p TYPED%-Closures, @p UntypeClosures lowers @p TYPED
/// to @p UNTYPED closures
/// Further, closures literals can have to forms:
/// - Lambdas: <code>(env, lambda)</code>
/// - Folded branches: <code>(proj i (env_0, .., env_N), proj i (lam_0, .., lam_N))</code>
/// The later form is introduced by the @p UnboxClosures pass.

class ClosureLit {
public:
    enum Kind { TYPED, UNTYPED };

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
    ///}

    operator bool() const {
        return def_ != nullptr;
    }

    operator const Tuple*() {
        return def_;
    }

    /// @name Properties
    /// @{
    unsigned int order() {
        return old_type()->order();
    }

    bool is_basicblock() {
        return old_type()->is_basicblock();
    }
    /// @}

    // @name Escape analyses
    /// @{
    bool marked_no_esc(); 
    static const Def* get_esc_annot(const Def*);
    /// @}

private:
    ClosureLit(const Tuple* def, ClosureLit::Kind kind)
        : kind_(kind), def_(def)
    {};

    const Pi* old_type();

    const Kind kind_;
    const Tuple* def_;

    friend ClosureLit isa_closure_lit(const Def* def, ClosureLit::Kind kind);
};

/// the type for the the environment of untyped closures
const Def* closure_uenv_type(World& world);

/// return @p def if @p def is a closure and @c nullptr otherwise
const Sigma* isa_ctype(const Def* def, ClosureLit::Kind kind = ClosureLit::TYPED);

/// creates a closure type from a @ Pi
Sigma* ctype(const Pi* pi);

/// tries to match a closure literal
ClosureLit isa_closure_lit(const Def* def, ClosureLit::Kind kind = ClosureLit::TYPED);

/// pack a typed closure. This assumes that @p fn expects the environment as its first argument.
const Def* pack_closure_dbg(const Def* env, const Def* fn, const Def* dbg, const Def* ct = nullptr);
inline const Def* pack_closure(const Def* env, const Def* fn, const Def* ct = nullptr) {
    return pack_closure_dbg(env, fn, nullptr, ct);
}

// FIXME: Implement this once the dependet typing works properly (see the App-case in rewrite())
// const Def* apply_closure(const Def* closure, Defs args);
// inline const Def* apply_closure(const Def* closure, const Tuple* args) {
//     return apply_closure(closure, args->ops());
// }

};

#endif
