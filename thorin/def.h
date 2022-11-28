#pragma once

#include <optional>
#include <vector>

#include "thorin/debug.h"

#include "thorin/util/array.h"
#include "thorin/util/cast.h"
#include "thorin/util/container.h"
#include "thorin/util/hash.h"
#include "thorin/util/print.h"

// clang-format off
#define THORIN_NODE(m)                                                        \
    m(Type, type)       m(Univ, univ)                                         \
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
enum : node_t { THORIN_NODE(CODE) Max };
#undef CODE
} // namespace Node

class App;
class Axiom;
class Var;
class Def;
class World;

using Defs     = Span<const Def*>;
using DefArray = Array<const Def*>;

//------------------------------------------------------------------------------

template<class T = u64>
std::optional<T> isa_lit(const Def*);
template<class T = u64>
T as_lit(const Def* def);

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
enum class Sort { Term, Type, Kind, Space, Univ };

//------------------------------------------------------------------------------

struct Dep {
    enum : unsigned {
        None  = 0,
        Axiom = 1 << 0,
        Proxy = 1 << 1,
        Nom   = 1 << 2,
        Var   = 1 << 3,
    };
};

/// Use as mixin to wrap all kind of Def::proj and Def::projs variants.
#define THORIN_PROJ(NAME, CONST)                                                                                   \
    nat_t num_##NAME##s() CONST { return ((const Def*)NAME())->num_projs(); }                                      \
    const Def* NAME(nat_t a, nat_t i, const Def* dbg = {}) CONST { return ((const Def*)NAME())->proj(a, i, dbg); } \
    const Def* NAME(nat_t i, const Def* dbg = {}) CONST { return ((const Def*)NAME())->proj(i, dbg); }             \
    template<nat_t A = -1_s, class F>                                                                              \
    auto NAME##s(F f, Defs dbgs = {}) CONST {                                                                      \
        return ((const Def*)NAME())->projs<A, F>(f, dbgs);                                                         \
    }                                                                                                              \
    template<nat_t A = -1_s>                                                                                       \
    auto NAME##s(Defs dbgs = {}) CONST {                                                                           \
        return ((const Def*)NAME())->projs<A>(dbgs);                                                               \
    }                                                                                                              \
    template<class F>                                                                                              \
    auto NAME##s(nat_t a, F f, Defs dbgs = {}) CONST {                                                             \
        return ((const Def*)NAME())->projs<F>(a, f, dbgs);                                                         \
    }                                                                                                              \
    auto NAME##s(nat_t a, Defs dbgs = {}) CONST { return ((const Def*)NAME())->projs(a, dbgs); }

/// Base class for all Def%s.
/// The data layout (see World::alloc and Def::partial_ops) looks like this:
/// ```
/// Def| dbg |type | op(0) ... op(num_ops-1) |
///    |--------------partial_ops------------|
///                |-------extended_ops------|
/// ```
/// @attention This means that any subclass of Def **must not** introduce additional members.
class Def : public RuntimeCast<Def> {
public:
    using NormalizeFn = const Def* (*)(const Def*, const Def*, const Def*, const Def*);

private:
    Def& operator=(const Def&) = delete;
    Def(const Def&)            = delete;

protected:
    /// Constructor for a structural Def.
    Def(World*, node_t, const Def* type, Defs ops, flags_t flags, const Def* dbg);
    Def(node_t n, const Def* type, Defs ops, flags_t flags, const Def* dbg);
    /// Constructor for a *nom*inal Def.
    Def(node_t, const Def* type, size_t num_ops, flags_t flags, const Def* dbg);
    virtual ~Def() = default;

public:
    /// @name getters
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

    /// Yields the **raw** type of this Def; maybe `nullptr`. @sa Def::unfold_type.
    const Def* type() const { return type_; }
    /// Yields the type of this Def and unfolds it if necessary. See Def::type, Def::reduce_rec.
    const Def* unfold_type() const;
    Sort sort() const;
    const Def* arity() const;
    ///@}

    /// @name ops
    ///@{
    template<size_t N = -1_s>
    auto ops() const {
        if constexpr (N == -1_s) {
            return Defs(num_ops_, ops_ptr());
        } else {
            return Span<const Def*>(N, ops_ptr()).template to_array<N>();
        }
    }
    const Def* op(size_t i) const { return ops()[i]; }
    size_t num_ops() const { return num_ops_; }
    ///@}

    /// @name set/unset ops (nominals only)
    ///@{
    /// You are supposed to set operands from left to right.
    /// You can change operands later on or even Def::unset them.
    /// @warning But Thorin assumes that a nominal is "final" if its last operands is set.
    /// So, don't set the last operand and leave the first one unset.
    Def* set(size_t i, const Def* def);
    Def* set(Defs ops) {
        for (size_t i = 0, e = num_ops(); i != e; ++i) set(i, ops[i]);
        return this;
    }

    void unset(size_t i);
    void unset() {
        for (size_t i = 0, e = num_ops(); i != e; ++i) unset(i);
    }
    Def* set_type(const Def*);
    void unset_type();

    /// Yields `true` if empty or the last op is set.
    bool is_set() const;
    ///@}

    /// @name extended_ops
    ///@{
    /// Includes Def::dbg (if not `nullptr`), Def::type() (if not `nullptr`),
    /// and then the other Def::ops() (if Def::is_set) in this order.
    Defs extended_ops() const;
    const Def* extended_op(size_t i) const { return extended_ops()[i]; }
    size_t num_extended_ops() const { return extended_ops().size(); }
    ///@}

    /// @name partial_ops
    ///@{
    /// Includes Def::dbg, Def::type, and Def::ops (in this order).
    /// Also works with partially set Def%s and doesn't assert.
    /// Unset operands are `nullptr`.
    Defs partial_ops() const { return Defs(num_ops_ + 2, ops_ptr() - 2); }
    const Def* partial_op(size_t i) const { return partial_ops()[i]; }
    size_t num_partial_ops() const { return partial_ops().size(); }
    ///@}

    /// @name uses
    ///@{
    const Uses& uses() const { return uses_; }
    Array<Use> copy_uses() const { return Array<Use>(uses_.begin(), uses_.end()); }
    size_t num_uses() const { return uses().size(); }
    ///@}

    /// @name dependence checks
    ///@{
    /// @see Dep.
    unsigned dep() const { return dep_; }
    bool dep_none() const { return dep() == Dep::None; }
    bool dep_const() const { return !(dep() & (Dep::Nom | Dep::Var)); }
    bool dep_proxy() const { return dep_ & Dep::Proxy; }
    ///@}

    /// @name proj
    ///@{
    /// Splits this Def via Extract%s or directly accessing the Def::ops in the case of Sigma%s or Arr%ays.

    /// Yields Def::arity as_lit, if it is in fact a Lit, or `1` otherwise.
    nat_t num_projs() const {
        if (auto a = isa_lit(arity())) return *a;
        return 1;
    }
    /// Similar to World::extract while assuming an arity of @p a but also works on Sigma%s, and Arr%ays.
    /// If `this` is a Sort::Term (see Def::sort), Def::proj resorts to World::extract.
    const Def* proj(nat_t a, nat_t i, const Def* dbg = {}) const;

    /// Same as above but takes Def::num_projs as arity.
    const Def* proj(nat_t i, const Def* dbg = {}) const { return proj(num_projs(), i, dbg); }

    /// Splits this Def via Def::proj%ections into an Arr%ay (if `A == -1_s`) or `std::array` (otherwise).
    /// Applies @p f to each element.
    /// ```
    /// std::array<const Def*, 2> ab = def->projs<2>();
    /// std::array<u64, 2>        xy = def->projs<2>(as_lit<nat_t>);
    /// auto [a, b] = def->projs<2>();
    /// auto [x, y] = def->projs<2>(as_lit<nat_t>);
    /// Array<const Def*> projs = def->projs();               // projs has def->num_projs() many elements
    /// Array<const Def*> projs = def->projs(n);              // projs has n elements - asserts if incorrect
    /// Array<const Lit*> lits = def->projs(as_lit<nat_t>);   // same as above but applies as_lit<nat_t> to each element
    /// Array<const Lit*> lits = def->projs(n, as_lit<nat_t>);// same as above but applies as_lit<nat_t> to each element
    /// ```
    template<nat_t A = -1_s, class F>
    auto projs(F f, Defs dbgs = {}) const {
        using R = std::decay_t<decltype(f(this))>;
        if constexpr (A == -1_s) {
            return projs(num_projs(), f, dbgs);
        } else {
            assert(A == as_lit(arity()));
            std::array<R, A> array;
            for (nat_t i = 0; i != A; ++i) array[i] = f(proj(A, i, dbgs.empty() ? nullptr : dbgs[i]));
            return array;
        }
    }

    template<class F>
    auto projs(nat_t a, F f, Defs dbgs = {}) const {
        using R = std::decay_t<decltype(f(this))>;
        return Array<R>(a, [&](nat_t i) { return f(proj(a, i, dbgs.empty() ? nullptr : dbgs[i])); });
    }

    template<nat_t A = -1_s>
    auto projs(Defs dbgs = {}) const {
        return projs<A>([](const Def* def) { return def; }, dbgs);
    }
    auto projs(nat_t a, Defs dbgs = {}) const {
        return projs(
            a, [](const Def* def) { return def; }, dbgs);
    }
    ///@}

    /// @name externals
    ///@{
    bool is_external() const;
    bool is_internal() const { return !is_external(); } ///< *Not* Def::is_external.
    void make_external();
    void make_internal();
    ///@}

    /// @name debug
    ///@{
    const Def* dbg() const { return dbg_; } ///< Returns debug information as Def.
    Debug debug() const { return dbg_; }    ///< Returns the debug information as Debug.
    std::string name() const { return debug().name; }
    Loc loc() const { return debug().loc; }
    const Def* meta() const { return debug().meta; }
    void set_dbg(const Def* dbg) const { dbg_ = dbg; }
    /// Set Def::name in Debug build only; does nothing in Release build.
#ifndef NDEBUG
    void set_debug_name(std::string_view) const;
#else
    void set_debug_name(std::string_view) const {}
#endif
    std::string unique_name() const; ///< name + "_" + Def::gid
    ///@}

    /// @name casts
    ///@{
    // clang-format off
    template<class T = Def> const T* isa_structural() const { return isa_nom<T, true>(); }
    template<class T = Def> const T*  as_structural() const { return  as_nom<T, true>(); }
    // clang-format on

    /// If `this` is *nom*inal, it will cast constness away and perform a dynamic cast to @p T.
    template<class T = Def, bool invert = false>
    T* isa_nom() const {
        if constexpr (std::is_same<T, Def>::value)
            return nom_ ^ invert ? const_cast<Def*>(this) : nullptr;
        else
            return nom_ ^ invert ? const_cast<Def*>(this)->template isa<T>() : nullptr;
    }

    /// Asserts that @c this is a *nom*inal, casts constness away and performs a static cast to @p T (checked in Debug
    /// build).
    template<class T = Def, bool invert = false>
    T* as_nom() const {
        assert(nom_ ^ invert);
        if constexpr (std::is_same<T, Def>::value)
            return const_cast<Def*>(this);
        else
            return const_cast<Def*>(this)->template as<T>();
    }
    ///@}

    /// @name var
    ///@{
    /// Retrieve Var for *nominals*.
    const Var* var(const Def* dbg = {});
    THORIN_PROJ(var, )
    ///@}

    /// @name reduce
    ///@{
    /// Rewrites Def::ops by substituting `this` nominal's Var with @p arg.
    DefArray reduce(const Def* arg) const;
    DefArray reduce(const Def* arg);
    /// Transitively Def::reduce Lam%s, if `this` is an App.
    /// @returns the reduced body.
    const Def* reduce_rec() const;
    ///@}

    /// @name virtual methods
    ///@{
    virtual bool check() { return true; }
    virtual size_t first_dependend_op() { return 0; }
    virtual const Def* rebuild(World&, const Def*, Defs, const Def*) const { unreachable(); }
    /// Def::rebuild%s this Def while using @p new_op as substitute for its @p i'th Def::op
    const Def* refine(size_t i, const Def* new_op) const;
    virtual Def* stub(World&, const Def*, const Def*) { unreachable(); }
    virtual const Def* restructure() { return nullptr; }
    ///@}

    /// @name dump
    ///@{
    void dump() const;
    void dump(int max) const;
    void write(int max) const;
    void write(int max, const char* file) const;
    std::ostream& stream(std::ostream&, int max) const;
    friend std::ostream& operator<<(std::ostream&, const Def*);
    ///@}

protected:
    const Def** ops_ptr() const {
        return reinterpret_cast<const Def**>(reinterpret_cast<char*>(const_cast<Def*>(this + 1)));
    }
    void finalize();
    bool equal(const Def* other) const;

    union {
        NormalizeFn normalizer_; ///< Axiom%s use this member to store their normalizer.
        const Axiom* axiom_;     /// Curried App%s of Axiom%s use this member to propagate the Axiom.
        mutable World* world_;
    };

    flags_t flags_;
    uint8_t node_;
    unsigned nom_    : 1;
    unsigned dep_    : 4;
    unsigned pading_ : 3;
    u16 curry_;
    hash_t hash_;
    u32 gid_;
    u32 num_ops_;
    mutable Uses uses_;
    mutable const Def* dbg_;
    const Def* type_;

    friend class World;
    friend void swap(World&, World&);
};

template<class T>
const T* isa(flags_t f, const Def* def) {
    if (auto d = def->template isa<T>(); d && d->flags() == f) return d;
    return nullptr;
}

template<class T>
const T* as([[maybe_unused]] flags_t f, const Def* def) {
    assert(isa<T>(f, def));
    return def;
}

//------------------------------------------------------------------------------

template<class To>
using DefMap  = GIDMap<const Def*, To>;
using DefSet  = GIDSet<const Def*>;
using Def2Def = DefMap<const Def*>;
using DefDef  = std::tuple<const Def*, const Def*>;
using DefVec  = std::vector<const Def*>;

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

template<class To>
using DefDefMap = absl::flat_hash_map<DefDef, To, DefDefHash, DefDefEq>;
using DefDefSet = absl::flat_hash_set<DefDef, DefDefHash, DefDefEq>;

template<class To>
using NomMap  = GIDMap<Def*, To>;
using NomSet  = GIDSet<Def*>;
using Nom2Nom = NomMap<Def*>;

//------------------------------------------------------------------------------

class Var : public Def {
private:
    Var(const Def* type, Def* nom, const Def* dbg)
        : Def(Node, type, Defs{nom}, 0, dbg) {}

public:
    /// @name ops
    ///@{
    Def* nom() const { return op(0)->as_nom(); }
    ///@}
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Var;
    friend class World;
};

template<class To>
using VarMap  = GIDMap<const Var*, To>;
using VarSet  = GIDSet<const Var*>;
using Var2Var = VarMap<const Var*>;

class Univ : public Def {
private:
    Univ(World& world)
        : Def(&world, Node, nullptr, Defs{}, 0, nullptr) {}

public:
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Univ;
    friend class World;
};

using level_t = u64;

class Type : public Def {
private:
    Type(const Def* level, const Def* dbg)
        : Def(Node, nullptr, {level}, 0, dbg) {}

public:
    const Def* level() const { return op(0); }

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Type;
    friend class World;
};

class Lit : public Def {
private:
    Lit(const Def* type, flags_t val, const Def* dbg)
        : Def(Node, type, Defs{}, val, dbg) {}

public:
    template<class T = flags_t>
    T get() const {
        static_assert(sizeof(T) <= 8);
        return bitcast<T>(flags_);
    }

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Lit;
    friend class World;
};

template<class T>
std::optional<T> isa_lit(const Def* def) {
    if (def == nullptr) return {};
    if (auto lit = def->isa<Lit>()) return lit->get<T>();
    return {};
}

template<class T>
T as_lit(const Def* def) {
    return def->as<Lit>()->get<T>();
}

class Nat : public Def {
private:
    Nat(World& world);

public:
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Nat;
    friend class World;
};

/// A built-in constant of type `.Nat -> *`.
class Idx : public Def {
private:
    Idx(const Def* type)
        : Def(Node, type, Defs{}, 0, nullptr) {}

public:
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    /// Checks if @p def isa `.Idx s` and returns s or `nullptr` otherwise.
    static const Def* size(const Def* def);

    /// @name convert between Idx::size and bitwidth and vice versa
    ///@{
    // clang-format off
    static constexpr nat_t bitwidth2size(nat_t n) { assert(n != 0); return n == 64 ? 0 : (1_n << n); }
    static constexpr nat_t size2bitwidth(nat_t n) { return n == 0 ? 64 : std::bit_width(n - 1_n); }
    // clang-format on
    static std::optional<nat_t> size2bitwidth(const Def* size);
    ///@}

    static constexpr auto Node = Node::Idx;
    friend class World;
};

class Proxy : public Def {
private:
    Proxy(const Def* type, Defs ops, u32 pass, u32 tag, const Def* dbg)
        : Def(Node, type, ops, (u64(pass) << 32_u64) | u64(tag), dbg) {}

public:
    /// @name misc getters
    ///@{
    u32 pass() const { return u32(flags() >> 32_u64); } ///< IPass::index within PassMan.
    u32 tag() const { return u32(flags()); }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Proxy;
    friend class World;
};

/// This node is a hole in the IR that is inferred by its context later on.
/// It is modelled as a *nom*inal Def.
/// If inference was successful,
class Infer : public Def {
private:
    Infer(const Def* type, const Def* dbg)
        : Def(Node, type, 1, 0, dbg) {}

public:
    /// @name op
    ///@{
    const Def* op() const { return Def::op(0); }
    Infer* set(const Def* op) { return Def::set(0, op)->as<Infer>(); }
    ///@}

    /// @name virtual methods
    ///@{
    Infer* stub(World&, const Def*, const Def*) override;
    ///@}

    static constexpr auto Node = Node::Infer;
    friend class World;
};

/// @deprecated A global variable in the data segment.
/// A Global may be mutable or immutable.
/// @attention WILL BE REMOVED.
class Global : public Def {
private:
    Global(const Def* type, bool is_mutable, const Def* dbg)
        : Def(Node, type, 1, is_mutable, dbg) {}

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

    /// @name misc getters
    ///@{
    bool is_mutable() const { return flags(); }
    ///@}

    /// @name virtual methods
    ///@{
    Global* stub(World&, const Def*, const Def*) override;
    ///@}

    static constexpr auto Node = Node::Global;
    friend class World;
};

hash_t UseHash::operator()(Use use) const { return hash_combine(hash_begin(u16(use.index())), hash_t(use->gid())); }

//------------------------------------------------------------------------------

// TODO: move
/// Helper function to cope with the fact that normalizers take all arguments and not only its axiom arguments.
std::pair<const Def*, std::vector<const Def*>> collect_args(const Def* def);

} // namespace thorin
