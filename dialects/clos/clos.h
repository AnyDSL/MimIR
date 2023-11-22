#pragma once

#include "thorin/world.h"

#include "dialects/clos/autogen.h"

namespace thorin::clos {

/// @name Closures
///@{

/// Wrapper around a Def that can be used to match closures (see isa_clos_lit).
class ClosLit {
public:
    /// @name Getters
    ///@{
    const Sigma* type() {
        assert(def_);
        return def_->type()->isa<Sigma>();
    }

    Ref env();
    Ref env_type() { return env()->type(); }

    Ref fnc();
    const Pi* fnc_type() { return fnc()->type()->isa<Pi>(); }
    Lam* fnc_as_lam();

    Ref env_var();
    Ref ret_var() { return fnc_as_lam()->ret_var(); }
    ///@}

    explicit operator bool() const { return def_ != nullptr; }
    operator const Tuple*() { return def_; }

    const Tuple* operator->() {
        assert(def_);
        return def_;
    }

    /// @name Properties
    ///@{
    bool is_returning() { return Pi::isa_returning(fnc_type()); }
    bool is_basicblock() { return Pi::isa_basicblock(fnc_type()); }
    attr get() { return attr_; } ///< Clos annotation. These should appear in front of the code-part.
    ///@}

private:
    ClosLit(const Tuple* def, attr a = attr::bot)
        : def_(def)
        , attr_(a) {}

    const Tuple* def_;
    const attr attr_;

    friend ClosLit isa_clos_lit(Ref, bool);
};

/// Tries to match a closure literal.
ClosLit isa_clos_lit(Ref def, bool fn_isa_lam = true);

/// Pack a typed closure. This assumes that @p fn expects the environment as its Clos_Env_Param%th argument.
Ref clos_pack(Ref env, Ref fn, Ref ct = nullptr);

/// Deconstruct a closure into `(env_type, function, env)`.
/// **Important**: use this or ClosLit to destruct closures, since typechecking dependent pairs is currently
/// broken.
std::tuple<Ref, Ref, Ref> clos_unpack(Ref c);

/// Apply a closure to arguments.
Ref clos_apply(Ref closure, Ref args);
inline Ref apply_closure(Ref closure, Defs args) {
    auto& w = closure->world();
    return clos_apply(closure, w.tuple(args));
}

// TODO: rename this
/// Checks is def is the variable of a mut of type N.
template<class N> std::tuple<const Extract*, N*> ca_isa_var(Ref def) {
    if (auto proj = def->isa<Extract>()) {
        if (auto var = proj->tuple()->isa<Var>(); var && var->mut()->isa<N>())
            return std::tuple(proj, var->mut()->as<N>());
    }
    return {nullptr, nullptr};
}
///@}

/// @name Closure Types
///@{

/// Returns @p def if @p def is a closure and @c nullptr otherwise
const Sigma* isa_clos_type(Ref def);

/// Creates a typed closure type from @p pi.
Sigma* clos_type(const Pi* pi);

/// Convert a closure type to a Pi, where the environment type has been removed or replaced by @p new_env_type
/// (if @p new_env_type != @c nullptr)
const Pi* clos_type_to_pi(Ref ct, Ref new_env_type = nullptr);

///@}

/// @name Closure Environment
///@{
/// `tup_or_sig` should generally be a Tuple, Sigma or Var.

/// Describes where the environment is placed in the argument list.
static constexpr size_t Clos_Env_Param = 1_u64;

// Adjust the index of an argument to account for the env param
inline size_t shift_env(size_t i) { return (i < Clos_Env_Param) ? i : i - 1_u64; }

// Same but skip the env param
inline size_t skip_env(size_t i) { return (i < Clos_Env_Param) ? i : i + 1_u64; }

// TODO what does this do exactly?
Ref ctype(World& w, Defs doms, Ref env_type = nullptr);

Ref clos_insert_env(size_t i, Ref env, std::function<Ref(size_t)> f);
inline Ref clos_insert_env(size_t i, Ref env, Ref a) {
    return clos_insert_env(i, env, [&](auto i) { return a->proj(i); });
}

inline Ref clos_insert_env(Ref env, Ref tup_or_sig) {
    auto& w = tup_or_sig->world();
    auto new_ops
        = vector<const Def*>(tup_or_sig->num_projs() + 1, [&](auto i) { return clos_insert_env(i, env, tup_or_sig); });
    return (tup_or_sig->isa<Sigma>()) ? w.sigma(new_ops) : w.tuple(new_ops);
}

Ref clos_remove_env(size_t i, std::function<Ref(size_t)> f);
inline Ref clos_remove_env(size_t i, Ref def) {
    return clos_remove_env(i, [&](auto i) { return def->proj(i); });
}
inline Ref clos_remove_env(Ref tup_or_sig) {
    auto& w = tup_or_sig->world();
    auto new_ops
        = vector<const Def*>(tup_or_sig->num_projs() - 1, [&](auto i) { return clos_remove_env(i, tup_or_sig); });
    return (tup_or_sig->isa<Sigma>()) ? w.sigma(new_ops) : w.tuple(new_ops);
}

inline Ref clos_sub_env(Ref tup_or_sig, Ref new_env) { return tup_or_sig->refine(Clos_Env_Param, new_env); }
///@}

} // namespace thorin::clos
