#pragma once

#include <optional>

#include <fe/assert.h>
#include <fe/cast.h>
#include <fe/enum.h>

#include "mim/config.h"

#include "mim/util/dbg.h"
#include "mim/util/hash.h"
#include "mim/util/pool.h"
#include "mim/util/util.h"
#include "mim/util/vector.h"

// clang-format off
#define MIM_NODE(m)                                                           \
    m(Type, type)       m(Univ, univ)   m(UMax, umax)       m(UInc, uinc)     \
    m(Pi, pi)           m(Lam, lam)     m(App, app)                           \
    m(Sigma, sigma)     m(Tuple, tuple) m(Extract, extract) m(Insert, insert) \
    m(Arr, arr)         m(Pack, pack)                                         \
    m(Join, join)       m(Vel, vel)     m(Test, test)       m(Top, top)       \
    m(Meet, meet)       m(Ac,  ac )     m(Pick, pick)       m(Bot, bot)       \
    m(Proxy, proxy)                                                           \
    m(Axiom, axiom)                                                           \
    m(Lit, lit)                                                               \
    m(Nat, nat)         m(Idx, idx)                                           \
    m(Var, var)                                                               \
    m(Infer, infer)                                                           \
    m(Global, global)                                                         \
    m(Uniq,   Uniq)
// clang-format on

namespace mim {

namespace Node {

#define CODE(node, name) node,
enum : node_t { MIM_NODE(CODE) };
#undef CODE

#define CODE(node, name) +size_t(1)
constexpr auto Num_Nodes = size_t(0) MIM_NODE(CODE);
#undef CODE

} // namespace Node

class App;
class Axiom;
class Var;
class Def;
class World;

/// @name Def
/// GIDSet / GIDMap keyed by Def::gid of `conset Def*`.
///@{
template<class To> using DefMap = GIDMap<const Def*, To>;
using DefSet                    = GIDSet<const Def*>;
using Def2Def                   = DefMap<const Def*>;
using Defs                      = View<const Def*>;
using DefVec                    = Vector<const Def*>;
///@}

/// @name Def (Mutable)
/// GIDSet / GIDMap keyed by Def::gid of `Def*`.
///@{
template<class To> using MutMap = GIDMap<Def*, To>;
using MutSet                    = GIDSet<Def*>;
using Mut2Mut                   = MutMap<Def*>;
using Muts                      = PooledSet<Def*>;
///@}

/// @name Var
/// GIDSet / GIDMap keyed by Var::gid of `const Var*`.
///@{
template<class To> using VarMap = GIDMap<const Var*, To>;
using VarSet                    = GIDSet<const Var*>;
using Var2Var                   = VarMap<const Var*>;
using Vars                      = PooledSet<const Var*>;
///@}

//------------------------------------------------------------------------------

/// Helper class to retrieve Infer::arg if present.
class Ref {
public:
    Ref() = default;
    Ref(const Def* def)
        : def_(def) {}

    const Def* operator*() const { return refer(def_); }
    const Def* operator->() const { return refer(def_); }
    operator const Def*() const { return refer(def_); }
    explicit operator bool() const { return def_; }
    static const Def* refer(const Def* def); ///< Same as Infer::find but does nothing if @p def is `nullptr`.
    const Def* def() const { return def_; }  ///< Retrieve wrapped Def without Infer::refer%ing.

    friend std::ostream& operator<<(std::ostream&, Ref);

private:
    const Def* def_ = nullptr;
};

using NormalizeFn = Ref (*)(Ref, Ref, Ref);

//------------------------------------------------------------------------------

// TODO remove or fix this
enum class Sort { Term, Type, Kind, Space, Univ, Level };

//------------------------------------------------------------------------------

using fe::operator&;
using fe::operator|;
using fe::operator^;
using fe::operator<=>;
using fe::operator==;
using fe::operator!=;

/// @name Dep
///@{
enum class Dep : unsigned {
    None  = 0,
    Axiom = 1 << 0,
    Infer = 1 << 1,
    Mut   = 1 << 2,
    Proxy = 1 << 3,
    Var   = 1 << 4,
};
///@}

} // namespace mim
#ifndef DOXYGEN
template<> struct fe::is_bit_enum<mim::Dep> : std::true_type {};
#endif

namespace mim {

/// Use as mixin to wrap all kind of Def::proj and Def::projs variants.
#define MIM_PROJ(NAME, CONST)                                                                                 \
    nat_t num_##NAME##s() CONST { return ((const Def*)NAME())->num_projs(); }                                 \
    nat_t num_t##NAME##s() CONST { return ((const Def*)NAME())->num_tprojs(); }                               \
    Ref NAME(nat_t a, nat_t i) CONST { return ((const Def*)NAME())->proj(a, i); }                             \
    Ref NAME(nat_t i) CONST { return ((const Def*)NAME())->proj(i); }                                         \
    Ref t##NAME(nat_t i) CONST { return ((const Def*)NAME())->tproj(i); }                                     \
    template<nat_t A = std::dynamic_extent, class F> auto NAME##s(F f) CONST {                                \
        return ((const Def*)NAME())->projs<A, F>(f);                                                          \
    }                                                                                                         \
    template<class F> auto t##NAME##s(F f) CONST { return ((const Def*)NAME())->tprojs<F>(f); }               \
    template<nat_t A = std::dynamic_extent> auto NAME##s() CONST { return ((const Def*)NAME())->projs<A>(); } \
    auto t##NAME##s() CONST { return ((const Def*)NAME())->tprojs(); }                                        \
    template<class F> auto NAME##s(nat_t a, F f) CONST { return ((const Def*)NAME())->projs<F>(a, f); }       \
    auto NAME##s(nat_t a) CONST { return ((const Def*)NAME())->projs(a); }

/// CRTP-based Mixin to declare setters for Def::loc \& Def::name using a *covariant* return type.
template<class P, class D = Def> class Setters { // D is only needed to make the resolution `D::template set` lazy
private:
    P* super() { return static_cast<P*>(this); }
    const P* super() const { return static_cast<const P*>(this); }

public:
    // clang-format off
    template<bool Ow = false> const P* set(Loc l               ) const { super()->D::template set<Ow>(l); return super(); }
    template<bool Ow = false>       P* set(Loc l               )       { super()->D::template set<Ow>(l); return super(); }
    template<bool Ow = false> const P* set(       Sym s        ) const { super()->D::template set<Ow>(s); return super(); }
    template<bool Ow = false>       P* set(       Sym s        )       { super()->D::template set<Ow>(s); return super(); }
    template<bool Ow = false> const P* set(       std::string s) const { super()->D::template set<Ow>(std::move(s)); return super(); }
    template<bool Ow = false>       P* set(       std::string s)       { super()->D::template set<Ow>(std::move(s)); return super(); }
    template<bool Ow = false> const P* set(Loc l, Sym s        ) const { super()->D::template set<Ow>(l, s); return super(); }
    template<bool Ow = false>       P* set(Loc l, Sym s        )       { super()->D::template set<Ow>(l, s); return super(); }
    template<bool Ow = false> const P* set(Loc l, std::string s) const { super()->D::template set<Ow>(l, std::move(s)); return super(); }
    template<bool Ow = false>       P* set(Loc l, std::string s)       { super()->D::template set<Ow>(l, std::move(s)); return super(); }
    template<bool Ow = false> const P* set(Dbg d               ) const { super()->D::template set<Ow>(d); return super(); }
    template<bool Ow = false>       P* set(Dbg d               )       { super()->D::template set<Ow>(d); return super(); }
    // clang-format on
};

/// Base class for all Def%s.
///
/// These are the most important subclasses:
/// | Type Formation    | Term Introduction | Term Elimination  |
/// | ----------------- | ----------------- | ----------------- |
/// | Pi                | Lam               | App               |
/// | Sigma / Arr       | Tuple / Pack      | Extract           |
/// |                   | Insert            | Insert            |
/// | Uniq              | Wrap              | Unwrap            |
/// | Join              | Vel               | Test              |
/// | Meet              | Ac                | Pick              |
/// | Nat               | Lit               |                   |
/// | Idx               | Lit               |                   |
/// In addition there is:
/// * Var: A variable. Currently the following Def%s may be binders:
///     * Pi, Lam, Sigma, Arr, Pack
/// * Axiom: To introduce new entities.
/// * Proxy: Used for intermediate values during optimizations.
/// * Infer: Hole in the presentation filled by type inference (always mutable as the holes are filled in later).
/// * Type, Univ, UMax, UInc: To keep track of type levels.
///
/// The data layout (see World::alloc and Def::deps) looks like this:
/// ```
/// Def| type | op(0) ... op(num_ops-1) |
///           |-----------ops-----------|
///                  deps
///    |--------------------------------| if type() != nullptr &&  is_set()
///           |-------------------------| if type() == nullptr &&  is_set()
///    |------|                           if type() != nullptr && !is_set()
///    ||                                 if type() == nullptr && !is_set()
/// ```
/// @attention This means that any subclass of Def **must not** introduce additional members.
/// @see @ref mut
class Def : public fe::RuntimeCast<Def> {
private:
    Def& operator=(const Def&) = delete;
    Def(const Def&)            = delete;

protected:
    /// @name C'tors and D'tors
    ///@{
    Def(World*, node_t, const Def* type, Defs ops, flags_t flags); ///< Constructor for an *immutable* Def.
    Def(node_t n, const Def* type, Defs ops, flags_t flags);       ///< As above but World retrieved from @p type.
    Def(node_t, const Def* type, size_t num_ops, flags_t flags);   ///< Constructor for a *mutable* Def.
    virtual ~Def() = default;
    ///@}

public:
    /// @name Getters
    ///@{
    World& world() const;
    flags_t flags() const { return flags_; }
    u32 gid() const { return gid_; }
    size_t hash() const { return hash_; }
    node_t node() const { return node_; }
    std::string_view node_name() const;
    ///@}

    /// @name type
    ///@{

    /// Yields the **raw** type of this Def, i.e. maybe `nullptr`. @see Def::unfold_type.
    Ref type() const { return type_; }
    /// Yields the type of this Def and builds a new `Type (UInc n)` if necessary.
    Ref unfold_type() const;
    /// Yields `true` if `this:T` and `T:(Type 0)`.
    bool is_term() const;
    ///@}

    /// @name arity
    ///@{
    Ref arity() const;
    std::optional<nat_t> isa_lit_arity() const;
    nat_t as_lit_arity() const {
        auto a = isa_lit_arity();
        assert(a.has_value());
        return *a;
    }
    ///@}

    /// @name ops
    ///@{
    template<size_t N = std::dynamic_extent> auto ops() const { return View<const Def*, N>(ops_ptr(), num_ops_); }
    Ref op(size_t i) const { return ops()[i]; }
    size_t num_ops() const { return num_ops_; }
    ///@}

    /// @name Setting Ops (Mutables Only)
    /// @anchor set_ops
    /// You can set and change the Def::ops of a mutable after construction.
    /// However, you have to obey the following rules:
    /// 1. If Def::is_set() is ...
    ///     1. ... `false`, [set](@ref Def::set) the [operands](@ref Def::ops) from
    ///         * left (`i == 0`) to
    ///         * right (`i == num_ops() - 1`).
    ///     2. ... `true`, [reset](@ref Def::reset) the operands from left to right as in 1a.
    /// 2. In addition, you can invoke Def::unset() at *any time* to start over with 1a:
    /// ```
    /// mut->unset()->set({a, b, c}); // This will always work, but should be your last resort.
    /// ```
    ///
    /// MimIR assumes that a mutable is *final*, when its last operand is set.
    /// Then, Def::check() will be invoked.
    ///@{
    Def* set(size_t i, Ref);                                        ///< Successively   set from left to right.
    Def* reset(size_t i, Ref def) { return unset(i)->set(i, def); } ///< Successively reset from left to right.
    Def* set(Defs ops);                                             ///< Def::set @p ops all at once.
    Def* reset(Defs ops);                                           ///< Def::reset @p ops all at once.
    Def* unset();        ///< Unsets all Def::ops; works even, if not set at all or partially.
    bool is_set() const; ///< Yields `true` if empty or the last op is set.
    ///@}

    /// @name deps
    /// All *dependencies* of a Def and includes:
    /// * Def::type() (if not `nullptr`) and
    /// * the other Def::ops() (only included, if Def::is_set()) in this order.
    ///@{
    Defs deps() const;
    Ref dep(size_t i) const { return deps()[i]; }
    size_t num_deps() const { return deps().size(); }
    ///@}

    /// @name has_dep
    /// Checks whether one Def::deps() contains specific elements defined in Dep.
    /// This works up to the next *mutable*.
    /// For example, consider the Tuple `tup`: `(<infer>, lam (x: Nat) = y)`:
    /// ```
    /// bool has_infer = tup->has_dep(Dep::Infer); // true;
    /// bool has_mut   = tup->has_dep(Dep::Mut);   // true;
    /// bool has_var   = tup->has_dep(Dep::Var);   // false - y is contained in another mutable;
    /// ```
    ///@{
    bool has_dep() const { return dep_ != 0; }
    bool has_dep(Dep d) const { return has_dep(unsigned(d)); }
    bool has_dep(unsigned u) const { return dep_ & u; }
    /// Neither a Dep::Mut nor a Dep::Var; can often be used as shortcut as an optimization.
    bool has_const_dep() const { return !has_dep(Dep::Mut | Dep::Var); }
    ///@}

    /// @name proj
    /// @anchor proj
    /// Splits this Def via Extract%s or directly accessing the Def::ops in the case of Sigma%s or Arr%ays.
    /// ```
    /// std::array<const Def*, 2> ab = def->projs<2>();
    /// std::array<u64, 2>        xy = def->projs<2>([](auto def) { return Lit::as(def); });
    /// auto [a, b]                  = def->projs<2>();
    /// auto [x, y]                  = def->projs<2>([](auto def) { return Lit::as(def); });
    /// Vector<const Def*> projs1    = def->projs(); // "projs1" has def->num_projs() many elements
    /// Vector<const Def*> projs2    = def->projs(n);// "projs2" has n elements - asserts if incorrect
    /// // same as above but applies Lit::as<nat_t>(def) to each element
    /// Vector<const Lit*> lits1     = def->projs(   [](auto def) { return Lit::as(def); });
    /// Vector<const Lit*> lits2     = def->projs(n, [](auto def) { return Lit::as(def); });
    /// ```
    ///@{

    /// Yields Def::as_lit_arity(), if it is in fact a Lit, or `1` otherwise.
    nat_t num_projs() const { return isa_lit_arity().value_or(1); }
    nat_t num_tprojs() const; ///< As above but yields 1, if Flags::scalarize_threshold is exceeded.

    /// Similar to World::extract while assuming an arity of @p a, but also works on Sigma%s and Arr%ays.
    Ref proj(nat_t a, nat_t i) const;
    Ref proj(nat_t i) const { return proj(num_projs(), i); }   ///< As above but takes Def::num_projs as arity.
    Ref tproj(nat_t i) const { return proj(num_tprojs(), i); } ///< As above but takes Def::num_tprojs.

    /// Splits this Def via Def::proj%ections into an Array (if `A == std::dynamic_extent`) or `std::array` (otherwise).
    /// Applies @p f to each element.
    template<nat_t A = std::dynamic_extent, class F> auto projs(F f) const {
        using R = std::decay_t<decltype(f(this))>;
        if constexpr (A == std::dynamic_extent) {
            return projs(num_projs(), f);
        } else {
            assert(A == as_lit_arity());
            std::array<R, A> array;
            for (nat_t i = 0; i != A; ++i) array[i] = f(proj(A, i));
            return array;
        }
    }

    template<class F> auto tprojs(F f) const { return projs(num_tprojs(), f); }

    template<class F> auto projs(nat_t a, F f) const {
        using R = std::decay_t<decltype(f(this))>;
        return Vector<R>(a, [&](nat_t i) { return f(proj(a, i)); });
    }
    template<nat_t A = std::dynamic_extent> auto projs() const {
        return projs<A>([](Ref def) { return *def; });
    }
    auto tprojs() const {
        return tprojs([](Ref def) { return *def; });
    }
    auto projs(nat_t a) const {
        return projs(a, [](Ref def) { return *def; });
    }
    ///@}

    /// @name var
    /// @anchor var
    /// Retrieve Var for *mutables*.
    /// @see @ref proj
    ///@{
    MIM_PROJ(var, )
    /// Not necessarily a Var: E.g., if the return type is `[]`, this will yield `()`.
    Ref var();
    /// Only returns not `nullptr`, if Var of this mutable has ever been created.
    const Var* has_var() { return var_; }
    /// As above if `this` is a *mutable*.
    const Var* has_var() const {
        if (auto mut = isa_mut()) return mut->has_var();
        return nullptr;
    }
    ///@}

    /// @name Free Vars and Muts
    /// * local_muts() / local_vars() are cached and hash-consed.
    /// * free_vars() are computed on demand and cached.
    ///   They will be transitively invalidated by following users, if a mutable is mutated.
    ///@{

    /// Mutables reachable by following *immutable* deps().
    /// @note `mut->local_muts()` is by definition the set `{ mut }`.
    Muts local_muts() const;
    /// Var%s reachable by following *immutable* deps().
    /// @note `var->local_vars()` is by definition the set `{ var }`.
    Vars local_vars() const { return mut_ ? Vars() : vars_.local; }
    /// Compute a global solution, i.e., by transitively following *mutables* as well.
    Vars free_vars() const;
    Vars free_vars();
    Muts users() { return muts_.users; } ///< Set of mutables where this mutable is locally referenced.
    bool is_open() const;                ///< Has free_vars()?
    bool is_closed() const;              ///< Has no free_vars()?
    ///@}

    /// @name external
    ///@{
    bool is_external() const { return external_; }
    void make_external();
    void make_internal();
    void transfer_external(Def* to) { make_internal(), to->make_external(); }
    ///@}

    /// @name Casts
    /// @see @ref cast_builtin
    ///@{
    // clang-format off
    template<class T = Def> const T* isa_imm() const { return isa_mut<T, true>(); }
    template<class T = Def> const T*  as_imm() const { return  as_mut<T, true>(); }
    template<class T = Def, class R> const T* isa_imm(R (T::*f)() const) const { return isa_mut<T, R, true>(f); }
    // clang-format on

    /// If `this` is *mut*able, it will cast `const`ness away and perform a `dynamic_cast` to @p T.
    template<class T = Def, bool invert = false> T* isa_mut() const {
        if constexpr (std::is_same<T, Def>::value)
            return mut_ ^ invert ? const_cast<Def*>(this) : nullptr;
        else
            return mut_ ^ invert ? const_cast<Def*>(this)->template isa<T>() : nullptr;
    }

    /// Asserts that `this` is a *mutable*, casts `const`ness away and performs a `static_cast` to @p T.
    template<class T = Def, bool invert = false> T* as_mut() const {
        assert(mut_ ^ invert);
        if constexpr (std::is_same<T, Def>::value)
            return const_cast<Def*>(this);
        else
            return const_cast<Def*>(this)->template as<T>();
    }
    ///@}

    /// @name Dbg Getters
    ///@{
    Dbg dbg() const { return dbg_; }
    Loc loc() const { return dbg_.loc(); }
    Sym sym() const { return dbg_.sym(); }
    std::string unique_name() const; ///< name + "_" + Def::gid
    ///@}

    /// @name Dbg Setters
    /// Every subclass `S` of Def has the same setters that return `S*`/`const S*` via the mixin Setters.
    ///@{
    // clang-format off
    template<bool Ow = false> const Def* set(Loc l) const { if (Ow || !dbg_.loc()) dbg_.set(l); return this; }
    template<bool Ow = false>       Def* set(Loc l)       { if (Ow || !dbg_.loc()) dbg_.set(l); return this; }
    template<bool Ow = false> const Def* set(Sym s) const { if (Ow || !dbg_.sym()) dbg_.set(s); return this; }
    template<bool Ow = false>       Def* set(Sym s)       { if (Ow || !dbg_.sym()) dbg_.set(s); return this; }
    template<bool Ow = false> const Def* set(       std::string s) const { set(sym(std::move(s))); return this; }
    template<bool Ow = false>       Def* set(       std::string s)       { set(sym(std::move(s))); return this; }
    template<bool Ow = false> const Def* set(Loc l, Sym s        ) const { set(l); set(s); return this; }
    template<bool Ow = false>       Def* set(Loc l, Sym s        )       { set(l); set(s); return this; }
    template<bool Ow = false> const Def* set(Loc l, std::string s) const { set(l); set(sym(std::move(s))); return this; }
    template<bool Ow = false>       Def* set(Loc l, std::string s)       { set(l); set(sym(std::move(s))); return this; }
    template<bool Ow = false> const Def* set(Dbg d) const { set(d.loc(), d.sym()); return this; }
    template<bool Ow = false>       Def* set(Dbg d)       { set(d.loc(), d.sym()); return this; }
    // clang-format on
    ///@}

    /// @name debug_prefix/suffix
    /// Prepends/Appends a prefix/suffix to Def::name - but only in `Debug` build.
    ///@{
#ifndef NDEBUG
    const Def* debug_prefix(std::string) const;
    const Def* debug_suffix(std::string) const;
#else
    const Def* debug_prefix(std::string) const { return this; }
    const Def* debug_suffix(std::string) const { return this; }
#endif
    ///@}

    /// @name Rebuild
    ///@{
    Def* stub(World& w, Ref type) { return stub_(w, type)->set(dbg()); }
    Def* stub(Ref type) { return stub(world(), type); }

    /// Def::rebuild%s this Def while using @p new_op as substitute for its @p i'th Def::op
    Ref rebuild(World& w, Ref type, Defs ops) const {
        assert(isa_imm());
        return rebuild_(w, type, ops)->set(dbg());
    }
    Ref rebuild(Ref type, Defs ops) const { return rebuild(world(), type, ops); }

    /// Tries to make an immutable from a mutable.
    /// This usually works if the mutable isn't recursive and its var isn't used.
    virtual const Def* immutabilize() { return nullptr; }
    bool is_immutabilizable();

    Ref refine(size_t i, Ref new_op) const;

    /// Rewrites Def::ops by substituting `this` mutable's Var with @p arg.
    DefVec reduce(Ref arg) const;
    DefVec reduce(Ref arg);
    /// As above but only rewrites `this->op(i)`.
    Ref reduce(size_t i, Ref arg) const;
    ///@}

    /// @name Type Checking
    ///@{
    virtual Ref check(size_t, Ref def) { return def; }
    virtual Ref check() { return type(); }
    const Def* zonk() const;
    ///@}

    /// @name dump
    ///@{
    void dump() const;
    void dump(int max) const;
    void write(int max) const;
    void write(int max, const char* file) const;
    std::ostream& stream(std::ostream&, int max) const;
    ///@}

    /// @name dot
    /// Dumps DOT to @p os while obeying maximum recursion depth of @p max.
    /// If @p types is `true`, Def::type() dependencies will be followed as well.
    ///@{
    void dot(std::ostream& os, uint32_t max = 0xFFFFFF, bool types = false) const;
    /// Same as above but write to @p file or `std::cout` if @p file is `nullptr`.
    void dot(const char* file = nullptr, uint32_t max = 0xFFFFFF, bool types = false) const;
    void dot(const std::string& file, uint32_t max = 0xFFFFFF, bool types = false) const {
        return dot(file.c_str(), max, types);
    }
    ///@}

protected:
    /// @name Wrappers for World::sym
    /// These are here to have Def::set%ters inline without including `mim/world.h`.
    ///@{
    Sym sym(const char*) const;
    Sym sym(std::string_view) const;
    Sym sym(std::string) const;
    ///@}

private:
    virtual Def* stub_(World&, Ref) { fe::unreachable(); }
    virtual Ref rebuild_(World& w, Ref type, Defs ops) const = 0;

    Vars free_vars(bool&, uint32_t run);
    void validate();
    void invalidate();
    Def* unset(size_t i);
    const Def** ops_ptr() const {
        return reinterpret_cast<const Def**>(reinterpret_cast<char*>(const_cast<Def*>(this + 1)));
    }
    bool equal(const Def* other) const;

protected:
    mutable Dbg dbg_;
    union {
        NormalizeFn normalizer_; ///< Axiom only: Axiom%s use this member to store their normalizer.
        const Axiom* axiom_;     ///< App only: Curried App%s of Axiom%s use this member to propagate the Axiom.
        const Var* var_;         ///< Mutable only: Var of a mutable.
        mutable World* world_;
    };
    flags_t flags_;
    u8 curry_ = 0;
    u8 trip_  = 0;

private:
    uint8_t node_;
    bool mut_      : 1;
    bool external_ : 1;
    unsigned dep_  : 5;
    bool valid_    : 1;
    uint32_t mark_ = 0;
#ifndef NDEBUG
    size_t curr_op_ = 0;
#endif

    u32 gid_;
    u32 num_ops_;
    size_t hash_;

    union LocalOrFreeVars {
        LocalOrFreeVars() {}

        Vars local; // Immutable only.
        Vars free;  // Mutable only.
    } vars_;

    union LocalOrConsumerMuts {
        LocalOrConsumerMuts() {}

        Muts local; // Immutable only.
        Muts users; // Mutable only.
    } muts_;

    const Def* type_;

    friend class World;
    friend void swap(World&, World&) noexcept;
    friend std::ostream& operator<<(std::ostream&, const Def*);
};

static_assert(sizeof(void*) != 8 || sizeof(Def) == 13 * 8, "make sure to not accidently increase sizeof(Def)");

/// @name DefDef
///@{
using DefDef = std::tuple<const Def*, const Def*>;

struct DefDefHash {
    size_t operator()(DefDef pair) const {
        if constexpr (sizeof(size_t) == 8)
            return hash((u64(std::get<0>(pair)->gid()) << 32_u64) | u64(std::get<1>(pair)->gid()));
        else
            return hash_combine(hash_begin(std::get<0>(pair)->gid()), std::get<1>(pair)->gid());
    }
};

struct DefDefEq {
    bool operator()(DefDef p1, DefDef p2) const { return p1 == p2; }
};

template<class To> using DefDefMap = absl::flat_hash_map<DefDef, To, DefDefHash, DefDefEq>;
using DefDefSet                    = absl::flat_hash_set<DefDef, DefDefHash, DefDefEq>;
///@}

class Var : public Def, public Setters<Var> {
private:
    Var(const Def* type, Def* mut)
        : Def(Node, type, Defs{mut}, 0) {}

public:
    using Setters<Var>::set;

    /// @name ops
    ///@{
    Def* mut() const { return op(0)->as_mut(); }
    ///@}

    static constexpr auto Node = Node::Var;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

class Univ : public Def, public Setters<Univ> {
public:
    using Setters<Univ>::set;
    static constexpr auto Node = Node::Univ;

private:
    Univ(World& world)
        : Def(&world, Node, nullptr, Defs{}, 0) {}

    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

class UMax : public Def, public Setters<UMax> {
public:
    using Setters<UMax>::set;
    static constexpr auto Node = Node::UMax;

private:
    UMax(World&, Defs ops);

    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

class UInc : public Def, public Setters<UInc> {
private:
    UInc(const Def* op, level_t offset)
        : Def(Node, op->type()->as<Univ>(), {op}, offset) {}

public:
    using Setters<UInc>::set;

    /// @name ops
    ///@{
    Ref op() const { return Def::op(0); }
    level_t offset() const { return flags(); }
    ///@}

    static constexpr auto Node = Node::UInc;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

class Type : public Def, public Setters<Type> {
private:
    Type(const Def* level)
        : Def(Node, nullptr, {level}, 0) {}

public:
    using Setters<Type>::set;

    /// @name ops
    ///@{
    Ref level() const { return op(0); }
    ///@}

    static constexpr auto Node = Node::Type;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

class Lit : public Def, public Setters<Lit> {
private:
    Lit(const Def* type, flags_t val)
        : Def(Node, type, Defs{}, val) {}

public:
    using Setters<Lit>::set;

    /// @name Get actual Constant
    ///@{
    template<class T = flags_t> T get() const {
        static_assert(sizeof(T) <= 8);
        return bitcast<T>(flags_);
    }
    ///@}

    using Def::as;
    using Def::isa;

    /// @name Casts
    ///@{
    /// @see @ref cast_lit
    template<class T = nat_t> static std::optional<T> isa(Ref def) {
        if (!def) return {};
        if (auto lit = def->isa<Lit>()) return lit->get<T>();
        return {};
    }
    template<class T = nat_t> static T as(Ref def) { return def->as<Lit>()->get<T>(); }
    ///@}

    static constexpr auto Node = Node::Lit;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

class Nat : public Def, public Setters<Nat> {
public:
    using Setters<Nat>::set;
    static constexpr auto Node = Node::Nat;

private:
    Nat(World& world);

    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

/// A built-in constant of type `Nat -> *`.
class Idx : public Def, public Setters<Idx> {
private:
    Idx(const Def* type)
        : Def(Node, type, Defs{}, 0) {}

public:
    using Setters<Idx>::set;
    using Def::as;
    using Def::isa;

    /// @name isa
    ///@{

    /// Checks if @p def is a `Idx s` and returns `s` or `nullptr` otherwise.
    static Ref isa(Ref def);
    static Ref as(Ref def) {
        auto res = isa(def);
        assert(res);
        return res;
    }
    static std::optional<nat_t> isa_lit(Ref def);
    static nat_t as_lit(Ref def) {
        auto res = isa_lit(def);
        assert(res.has_value());
        return *res;
    }
    ///@}

    /// @name Convert between Idx::isa and bitwidth and vice versa
    ///@{
    // clang-format off
    static constexpr nat_t bitwidth2size(nat_t n) { assert(n != 0); return n == 64 ? 0 : (1_n << n); }
    static constexpr nat_t size2bitwidth(nat_t n) { return n == 0 ? 64 : std::bit_width(n - 1_n); }
    // clang-format on
    static std::optional<nat_t> size2bitwidth(Ref size);
    ///@}

    static constexpr auto Node = Node::Idx;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

class Proxy : public Def, public Setters<Proxy> {
private:
    Proxy(const Def* type, Defs ops, u32 pass, u32 tag)
        : Def(Node, type, ops, (u64(pass) << 32_u64) | u64(tag)) {}

public:
    using Setters<Proxy>::set;

    /// @name Getters
    ///@{
    u32 pass() const { return u32(flags() >> 32_u64); } ///< IPass::index within PassMan.
    u32 tag() const { return u32(flags()); }
    ///@}

    static constexpr auto Node = Node::Proxy;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

/// @deprecated A global variable in the data segment.
/// A Global may be mutable or immutable.
/// @deprecated Will be removed.
class Global : public Def, public Setters<Global> {
private:
    Global(const Def* type, bool is_mutable)
        : Def(Node, type, 1, is_mutable) {}

public:
    using Setters<Global>::set;

    /// @name ops
    ///@{
    Ref init() const { return op(0); }
    void set(Ref init) { Def::set(0, init); }
    ///@}

    /// @name type
    ///@{
    const App* type() const;
    Ref alloced_type() const;
    ///@}

    /// @name Getters
    ///@{
    bool is_mutable() const { return flags(); }
    ///@}

    /// @name Rebuild
    ///@{
    Global* stub(Ref type) { return stub_(world(), type)->set(dbg()); }
    ///@}

    static constexpr auto Node = Node::Global;

private:
    Ref rebuild_(World&, Ref, Defs) const override;
    Global* stub_(World&, Ref) override;

    friend class World;
};

} // namespace mim
