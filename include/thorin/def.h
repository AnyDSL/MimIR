#pragma once

#include <optional>
#include <vector>

#include <fe/assert.h>
#include <fe/cast.h>

#include "thorin/config.h"

#include "thorin/util/dbg.h"
#include "thorin/util/hash.h"
#include "thorin/util/print.h"
#include "thorin/util/util.h"
#include "thorin/util/vector.h"

// clang-format off
#define THORIN_NODE(m)                                                        \
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
    m(Singleton, singleton)
// clang-format on

namespace thorin {

namespace Node {

#define CODE(node, name) node,
enum : node_t { THORIN_NODE(CODE) };
#undef CODE

#define CODE(node, name) +size_t(1)
constexpr auto Num_Nodes = size_t(0) THORIN_NODE(CODE);
#undef CODE

} // namespace Node

class App;
class Axiom;
class Var;
class Def;
class World;

/// @name Def
///@{
/// GIDSet / GIDMap keyed by Def::gid of `conset Def*`.
template<class To> using DefMap = GIDMap<const Def*, To>;
using DefSet                    = GIDSet<const Def*>;
using Def2Def                   = DefMap<const Def*>;
using Defs                      = View<const Def*>;
using DefVec                    = Vector<const Def*>;
///@}

/// @name Def (Mutable)
///@{
/// GIDSet / GIDMap keyed by Def::gid of `Def*`.
template<class To> using MutMap = GIDMap<Def*, To>;
using MutSet                    = GIDSet<Def*>;
using Mut2Mut                   = MutMap<Def*>;
///@}

/// @name Var
///@{
/// GIDSet / GIDMap keyed by Var::gid of `const Var*`.
template<class To> using VarMap = GIDMap<const Var*, To>;
using VarSet                    = GIDSet<const Var*>;
using Var2Var                   = VarMap<const Var*>;
///@{

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
    static const Def* refer(const Def* def); ///< Retrieves Infer::arg from @p def.

private:
    const Def* def_ = nullptr;
};

using NormalizeFn = Ref (*)(Ref, Ref, Ref);

//------------------------------------------------------------------------------

/// References a user.
/// A Def `u` which uses Def `d` as `i^th` operand is a Use with Use::index `i` of Def `d`.
class Use {
public:
    static constexpr size_t Type = -1_s;

    Use() {}
    Use(const Def* def, size_t index)
        : def_(def)
        , index_(index) {}

    size_t index() const { return index_; }
    const Def* def() const { return def_; }
    operator const Def*() const { return def_; }
    const Def* operator->() const { return def_; }
    bool operator==(Use other) const { return this->def_ == other.def_ && this->index_ == other.index_; }

private:
    const Def* def_;
    size_t index_;
};

struct UseHash {
    inline hash_t operator()(Use) const;
};

struct UseEq {
    bool operator()(Use u1, Use u2) const { return u1 == u2; }
};

using Uses = absl::flat_hash_set<Use, UseHash, UseEq>;

// TODO remove or fix this
enum class Sort { Term, Type, Kind, Space, Univ, Level };

//------------------------------------------------------------------------------

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

THORIN_ENUM_OPERATORS(Dep)
///@}

/// Use as mixin to wrap all kind of Def::proj and Def::projs variants.
#define THORIN_PROJ(NAME, CONST)                                                                              \
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

// clang-format off
/// Use as mixin to declare setters for Def::loc \& Def::name using a *covariant* return type.
#define THORIN_SETTERS_(T)                                                                                                 \
public:                                                                                                                    \
    template<bool Ow = false> const T* set(Loc l               ) const { if (Ow || !dbg_.loc) dbg_.loc = l; return this; } \
    template<bool Ow = false>       T* set(Loc l               )       { if (Ow || !dbg_.loc) dbg_.loc = l; return this; } \
    template<bool Ow = false> const T* set(       Sym s        ) const { if (Ow || !dbg_.sym) dbg_.sym = s; return this; } \
    template<bool Ow = false>       T* set(       Sym s        )       { if (Ow || !dbg_.sym) dbg_.sym = s; return this; } \
    template<bool Ow = false> const T* set(       std::string s) const {         set(sym(std::move(s))); return this; }    \
    template<bool Ow = false>       T* set(       std::string s)       {         set(sym(std::move(s))); return this; }    \
    template<bool Ow = false> const T* set(Loc l, Sym s        ) const { set(l); set(s);                 return this; }    \
    template<bool Ow = false>       T* set(Loc l, Sym s        )       { set(l); set(s);                 return this; }    \
    template<bool Ow = false> const T* set(Loc l, std::string s) const { set(l); set(sym(std::move(s))); return this; }    \
    template<bool Ow = false>       T* set(Loc l, std::string s)       { set(l); set(sym(std::move(s))); return this; }    \
    template<bool Ow = false> const T* set(Dbg d) const { set(d.loc, d.sym); return this; }                                \
    template<bool Ow = false>       T* set(Dbg d)       { set(d.loc, d.sym); return this; }
// clang-format on

#ifdef DOXYGEN
#    define THORIN_SETTERS(T) public: // Don't spam each and every sub class of Def with basically the same docs.
#else
#    define THORIN_SETTERS(T) THORIN_SETTERS_(T)
#endif

#define THORIN_DEF_MIXIN(T)                        \
public:                                            \
    THORIN_SETTERS(T)                              \
    Ref rebuild(World&, Ref, Defs) const override; \
    static constexpr auto Node = Node::T;          \
                                                   \
private:                                           \
    friend class World;

/// Base class for all Def%s.
/// The data layout (see World::alloc and Def::partial_ops) looks like this:
/// ```
/// Def| type | op(0) ... op(num_ops-1) |
///           |-----------ops-----------|
///    |----------partial_ops-----------|
///
///              extended_ops
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
    Def(World*, node_t, const Def* type, Defs ops, flags_t flags); ///< Constructor for an *immutable* Def.
    Def(node_t n, const Def* type, Defs ops, flags_t flags);
    Def(node_t, const Def* type, size_t num_ops, flags_t flags); ///< Constructor for a *mutable* Def.
    virtual ~Def() = default;

public:
    /// @name Getters
    ///@{
    World& world() const;
    flags_t flags() const { return flags_; }
    u32 gid() const { return gid_; }
    hash_t hash() const { return hash_; }
    node_t node() const { return node_; }
    std::string_view node_name() const;
    ///@}

    /// @name type
    ///@{

    /// Yields the **raw** type of this Def, i.e. maybe `nullptr`. @see Def::unfold_type.
    const Def* type() const { return type_; }
    /// Yields the type of this Def and builds a new `.Type (UInc n)` if necessary.
    const Def* unfold_type() const;
    /// Yields `true` if `this:T` and `T:(.Type 0)`.
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
    const Def* op(size_t i) const { return ops()[i]; }
    size_t num_ops() const { return num_ops_; }
    ///@}

    /// @name Setting Ops (Mutables Only)
    /// @anchor set_ops
    ///@{
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
    /// Thorin assumes that a mutable is *final*, when its last operand is set.
    /// Then, Def::check() will be invoked.
    Def* set(size_t i, const Def* def);                                    ///< Successively   set from left to right.
    Def* reset(size_t i, const Def* def) { return unset(i)->set(i, def); } ///< Successively reset from left to right.
    Def* set(Defs ops);                                                    ///< Def::set @p ops all at once.
    Def* reset(Defs ops);                                                  ///< Def::reset @p ops all at once.
    Def* unset(); ///< Unsets all Def::ops; works even, if not set at all or partially.
    Def* set_type(const Def*);
    void unset_type();

    /// Resolves Infer%s of this Def's type.
    void update() {
        if (auto r = Ref::refer(type()); r && r != type()) set_type(r);
    }

    /// Yields `true` if empty or the last op is set.
    bool is_set() const;
    ///@}

    /// @name extended_ops
    ///@{
    /// Includes Def::type() (if not `nullptr`) and then the other Def::ops() in this order.
    /// Def::ops() is only included, if Def::is_set.
    Defs extended_ops() const;
    const Def* extended_op(size_t i) const { return extended_ops()[i]; }
    size_t num_extended_ops() const { return extended_ops().size(); }
    ///@}

    /// @name partial_ops
    ///@{
    /// Includes Def::type() and then the other Def::ops() in this order.
    /// Also works with partially set Def%s and doesn't assert.
    /// Unset operands are `nullptr`.
    Defs partial_ops() const { return Defs(ops_ptr() - 1, num_ops_ + 1); }
    const Def* partial_op(size_t i) const { return partial_ops()[i]; }
    size_t num_partial_ops() const { return partial_ops().size(); }
    ///@}

    /// @name uses
    ///@{
    const Uses& uses() const { return uses_; }
    size_t num_uses() const { return uses().size(); }
    ///@}

    /// @name dep
    ///@{
    /// @see Dep.
    unsigned dep() const { return dep_; }
    bool has_dep(Dep d) const { return has_dep(unsigned(d)); }
    bool has_dep(unsigned u) const { return dep() & u; }
    bool dep_const() const { return !has_dep(Dep::Mut | Dep::Var); }
    ///@}

    /// @name proj
    /// @anchor proj
    ///@{
    /// Splits this Def via Extract%s or directly accessing the Def::ops in the case of Sigma%s or Arr%ays.
    /// ```
    /// std::array<const Def*, 2> ab = def->projs<2>();
    /// std::array<u64, 2>        xy = def->projs<2>([](auto def) { return Lit::as(def); });
    /// auto [a, b]                  = def->projs<2>();
    /// auto [x, y]                  = def->projs<2>([](auto def) { return Lit::as(def); });
    /// Array<const Def*> projs1     = def->projs(); // "projs1" has def->num_projs() many elements
    /// Array<const Def*> projs2     = def->projs(n);// "projs2" has n elements - asserts if incorrect
    /// // same as above but applies Lit::as<nat_t>(def) to each element
    /// Array<const Lit*> lits1      = def->projs(   [](auto def) { return Lit::as(def); });
    /// Array<const Lit*> lits2      = def->projs(n, [](auto def) { return Lit::as(def); });
    /// ```

    /// Yields Def::as_lit_arity(), if it is in fact a Lit, or `1` otherwise.
    nat_t num_projs() const { return isa_lit_arity().value_or(1); }
    nat_t num_tprojs() const; ///< As above but yields 1, if Flags::scalerize_threshold is exceeded.

    /// Similar to World::extract while assuming an arity of @p a, but also works on Sigma%s and Arr%ays.
    const Def* proj(nat_t a, nat_t i) const;

    const Def* proj(nat_t i) const { return proj(num_projs(), i); }   ///< As above but takes Def::num_projs as arity.
    const Def* tproj(nat_t i) const { return proj(num_tprojs(), i); } ///< As above but takes Def::num_tprojs.

    /// Splits this Def via Def::proj%ections into an Array (if `A == -1_n`) or `std::array` (otherwise).
    /// Applies @p f to each element.
    template<nat_t A = -1_n, class F> auto projs(F f) const {
        using R = std::decay_t<decltype(f(this))>;
        if constexpr (A == -1_n) {
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
    template<nat_t A = -1_n> auto projs() const {
        return projs<A>([](const Def* def) { return def; });
    }
    auto tprojs() const {
        return tprojs([](const Def* def) { return def; });
    }
    auto projs(nat_t a) const {
        return projs(a, [](const Def* def) { return def; });
    }
    ///@}

    /// @name var
    /// @anchor var
    ///@{
    /// Retrieve Var for *mutables*.
    /// @see @ref proj
    Ref var();
    const Var* true_var();
    THORIN_PROJ(var, )
    ///@}

    /// @name Free Vars and Muts
    ///@{
    const auto& local_muts() const { return local_muts_; }
    const auto& local_vars() const { return local_vars_; }
    VarSet free_vars() const;
    VarSet free_vars();
    VarSet free_vars(MutMap<VarSet>&);
    ///@}

    /// @name external
    ///@{
    bool is_external() const { return external_; }
    void make_external();
    void make_internal();
    void transfer_external(Def* to) { make_internal(), to->make_external(); }
    ///@}

    /// @name Casts
    ///@{
    /// @see @ref cast_builtin
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
    Loc loc() const { return dbg_.loc; }
    Sym sym() const { return dbg_.sym; }
    std::string unique_name() const; ///< name + "_" + Def::gid
    ///@}

    /// @name Dbg Setters
    ///@{
    /// Every subclass `S` of Def has the same setters that return `S*`/`const S*` but will not show up in Doxygen.
    THORIN_SETTERS_(Def)
    ///@}

    /// @name debug_prefix/suffix
    ///@{
    /// Prepends/Appends a prefix/suffix to Def::name - but only in `Debug` build.
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
    virtual Def* stub(World&, Ref) { fe::unreachable(); }

    /// Def::rebuild%s this Def while using @p new_op as substitute for its @p i'th Def::op
    virtual Ref rebuild(World& w, Ref type, Defs ops) const = 0;

    /// Tries to make an immutable from a mutable.
    /// This usually works if the mutable isn't recursive and its var isn't used.
    virtual const Def* immutabilize() { return nullptr; }

    const Def* refine(size_t i, const Def* new_op) const;

    /// Rewrites Def::ops by substituting `this` mutable's Var with @p arg.
    DefVec reduce(const Def* arg) const;
    DefVec reduce(const Def* arg);
    ///@}

    /// @name Type Checking
    ///@{
    virtual void check() {}
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
    ///@{
    /// Dumps DOT to @p os while obeying maximum recursion depth of @p max.
    /// If @p types is `true`, Def::type() dependencies will be followed as well.
    void dot(std::ostream& os, uint32_t max = 0xFFFFFF, bool types = false) const;
    /// Same as above but write to @p file or `std::cout` if @p file is `nullptr`.
    void dot(const char* file = nullptr, uint32_t max = 0xFFFFFF, bool types = false) const;
    ///@}

protected:
    /// @name Wrappers for World::sym
    ///@{
    /// These are here to have Def::set%ters inline without including `thorin/world.h`.
    Sym sym(const char*) const;
    Sym sym(std::string_view) const;
    Sym sym(std::string) const;
    ///@}

private:
    Def* unset(size_t i);
    const Def** ops_ptr() const {
        return reinterpret_cast<const Def**>(reinterpret_cast<char*>(const_cast<Def*>(this + 1)));
    }
    void finalize();
    bool equal(const Def* other) const;

#ifndef NDEBUG
    size_t curr_op_ = 0;
#endif

protected:
    mutable Dbg dbg_;
    union {
        NormalizeFn normalizer_; ///< Axiom%s use this member to store their normalizer.
        const Axiom* axiom_;     /// Curried App%s of Axiom%s use this member to propagate the Axiom.
        mutable World* world_;
    };
    flags_t flags_;
    u8 curry_;
    u8 trip_;

private:
    uint8_t node_;
    bool mut_      : 1;
    bool external_ : 1;
    unsigned dep_  : 5;
    bool padding_  : 1;
    hash_t hash_;
    u32 gid_;
    u32 num_ops_;
    mutable Uses uses_;
    VarSet local_vars_;
    MutSet local_muts_;
    const Def* type_;

    friend class World;
    friend void swap(World&, World&) noexcept;
};

/// @name std::ostream operator
///@{
std::ostream& operator<<(std::ostream&, const Def*);
std::ostream& operator<<(std::ostream&, Ref);
///@}

/// @name Formatted Output
///@{
/// Uses @p def->loc() as Loc%ation.
template<class T = std::logic_error, class... Args>
[[noreturn]] void error(const Def* def, const char* fmt, Args&&... args) {
    error(def->loc(), fmt, std::forward<Args&&>(args)...);
}
///@}

/// @name DefDef
///@{
using DefDef = std::tuple<const Def*, const Def*>;

struct DefDefHash {
    hash_t operator()(DefDef pair) const {
        hash_t hash = std::get<0>(pair)->gid();
        hash        = murmur3(hash, std::get<1>(pair)->gid());
        hash        = murmur3_finalize(hash, 8);
        return hash;
    }
};

struct DefDefEq {
    bool operator()(DefDef p1, DefDef p2) const { return p1 == p2; }
};

template<class To> using DefDefMap = absl::flat_hash_map<DefDef, To, DefDefHash, DefDefEq>;
using DefDefSet                    = absl::flat_hash_set<DefDef, DefDefHash, DefDefEq>;
///@}

class Var : public Def {
private:
    Var(const Def* type, Def* mut)
        : Def(Node, type, Defs{mut}, 0) {}

public:
    /// @name ops
    ///@{
    Def* mut() const { return op(0)->as_mut(); }
    ///@}

    THORIN_DEF_MIXIN(Var)
};

class Univ : public Def {
private:
    Univ(World& world)
        : Def(&world, Node, nullptr, Defs{}, 0) {}

    THORIN_DEF_MIXIN(Univ)
};

class UMax : public Def {
private:
    UMax(World&, Defs ops);

    THORIN_DEF_MIXIN(UMax)
};

class UInc : public Def {
private:
    UInc(const Def* op, level_t offset)
        : Def(Node, op->type()->as<Univ>(), {op}, offset) {}

public:
    /// @name ops
    ///@{
    const Def* op() const { return Def::op(0); }
    level_t offset() const { return flags(); }
    ///@}

    THORIN_DEF_MIXIN(UInc)
};

class Type : public Def {
private:
    Type(const Def* level)
        : Def(Node, nullptr, {level}, 0) {}

public:
    /// @name ops
    ///@{
    const Def* level() const { return op(0); }
    ///@}

    THORIN_DEF_MIXIN(Type)
};

class Lit : public Def {
private:
    Lit(const Def* type, flags_t val)
        : Def(Node, type, Defs{}, val) {}

public:
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

    THORIN_DEF_MIXIN(Lit)
};

class Nat : public Def {
private:
    Nat(World& world);

    THORIN_DEF_MIXIN(Nat)
};

/// A built-in constant of type `.Nat -> *`.
class Idx : public Def {
private:
    Idx(const Def* type)
        : Def(Node, type, Defs{}, 0) {}

public:
    /// Checks if @p def is a `.Idx s` and returns `s` or `nullptr` otherwise.
    static Ref size(Ref def);

    /// @name Convert between Idx::size and bitwidth and vice versa
    ///@{
    // clang-format off
    static constexpr nat_t bitwidth2size(nat_t n) { assert(n != 0); return n == 64 ? 0 : (1_n << n); }
    static constexpr nat_t size2bitwidth(nat_t n) { return n == 0 ? 64 : std::bit_width(n - 1_n); }
    // clang-format on
    static std::optional<nat_t> size2bitwidth(const Def* size);
    ///@}

    THORIN_DEF_MIXIN(Idx)
};

class Proxy : public Def {
private:
    Proxy(const Def* type, Defs ops, u32 pass, u32 tag)
        : Def(Node, type, ops, (u64(pass) << 32_u64) | u64(tag)) {}

public:
    /// @name Getters
    ///@{
    u32 pass() const { return u32(flags() >> 32_u64); } ///< IPass::index within PassMan.
    u32 tag() const { return u32(flags()); }
    ///@}

    THORIN_DEF_MIXIN(Proxy)
};

/// @deprecated A global variable in the data segment.
/// A Global may be mutable or immutable.
/// @attention WILL BE REMOVED.
class Global : public Def {
private:
    Global(const Def* type, bool is_mutable)
        : Def(Node, type, 1, is_mutable) {}

public:
    /// @name ops
    ///@{
    /// This thing's sole purpose is to differentiate on global from another.
    const Def* init() const { return op(0); }
    void set(const Def* init) { Def::set(0, init); }
    ///@}

    /// @name type
    ///@{
    const App* type() const;
    const Def* alloced_type() const;
    ///@}

    /// @name Getters
    ///@{
    bool is_mutable() const { return flags(); }
    ///@}

    Global* stub(World&, Ref) override;

    THORIN_DEF_MIXIN(Global)
};

hash_t UseHash::operator()(Use use) const { return hash_combine(hash_begin(u16(use.index())), hash_t(use->gid())); }

} // namespace thorin
