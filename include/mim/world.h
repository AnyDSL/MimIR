#pragma once

#include <string>
#include <string_view>
#include <type_traits>

#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>
#include <fe/arena.h>

#include "mim/axiom.h"
#include "mim/check.h"
#include "mim/flags.h"
#include "mim/lam.h"
#include "mim/lattice.h"
#include "mim/tuple.h"
#include "mim/util/dbg.h"
#include "mim/util/log.h"

namespace mim {
class Driver;

/// The World represents the whole program and manages creation of Thorin nodes (Def%s).
/// Def%s are hashed into an internal HashSet.
/// The World's factory methods just calculate a hash and lookup the Def, if it is already present, or create a new one
/// otherwise. This corresponds to value numbering.
///
/// You can create several worlds.
/// All worlds are completely independent from each other.
///
/// Note that types are also just Def%s and will be hashed as well.
class World {
public:
    /// @name State
    ///@{
    struct State {
        State() = default;

        /// [Plain Old Data](https://en.cppreference.com/w/cpp/named_req/PODType)
        struct POD {
            u32 curr_gid = 0;
            u32 curr_sub = 0;
            Loc loc;
            Sym name;
            mutable bool frozen = false;
        } pod;

#ifdef THORIN_ENABLE_CHECKS
        absl::flat_hash_set<uint32_t> breakpoints;
#endif
        friend void swap(State& s1, State& s2) noexcept {
            using std::swap;
            assert((!s1.pod.loc || !s2.pod.loc) && "Why is get_loc() still set?");
            swap(s1.pod, s2.pod);
#ifdef THORIN_ENABLE_CHECKS
            swap(s1.breakpoints, s2.breakpoints);
#endif
        }
    };

    /// @name C'tor and D'tor
    ///@{
    World& operator=(World) = delete;

    /// Inherits the @p state into the new World.
    explicit World(Driver*);
    World(Driver*, const State&);
    World(World&& other) noexcept
        : World(&other.driver(), other.state()) {
        swap(*this, other);
    }
    World inherit() { return World(&driver(), state()); }
    ~World();
    ///@}

    /// @name Getters/Setters
    ///@{
    const State& state() const { return state_; }

    const Driver& driver() const { return *driver_; }
    Driver& driver() { return *driver_; }

    Sym name() const { return state_.pod.name; }
    void set(Sym name) { state_.pod.name = name; }
    void set(std::string_view name) { state_.pod.name = sym(name); }

    /// Manage global identifier - a unique number for each Def.
    u32 curr_gid() const { return state_.pod.curr_gid; }
    u32 next_gid() { return ++state_.pod.curr_gid; }

    /// Retrive compile Flags.
    Flags& flags();
    ///@}

    /// @name Loc
    ///@{
    struct ScopedLoc {
        ScopedLoc(World& world, Loc old_loc)
            : world_(world)
            , old_loc_(old_loc) {}
        ~ScopedLoc() { world_.set_loc(old_loc_); }

    private:
        World& world_;
        Loc old_loc_;
    };

    Loc get_loc() const { return state_.pod.loc; }
    void set_loc(Loc loc = {}) { state_.pod.loc = loc; }
    ScopedLoc push(Loc loc) {
        auto sl = ScopedLoc(*this, get_loc());
        set_loc(loc);
        return sl;
    }
    ///@}

    /// @name Sym
    ///@{
    Sym sym(std::string_view);
    Sym sym(const char*);
    Sym sym(const std::string&);
    /// Appends a @p suffix or an increasing number if the suffix already exists.
    Sym append_suffix(Sym name, std::string suffix);
    ///@}

    /// @name Freeze
    ///@{
    /// In frozen state the World does not create any nodes.
    bool is_frozen() const { return state_.pod.frozen; }

    /// Yields old frozen state.
    bool freeze(bool on = true) const {
        bool old          = state_.pod.frozen;
        state_.pod.frozen = on;
        return old;
    }

    /// Use to World::freeze and automatically unfreeze at the end of scope.
    struct Freezer {
        Freezer(const World& world)
            : world(world)
            , old(world.freeze(true)) {}
        ~Freezer() { world.freeze(old); }

        const World& world;
        bool old;
    };
    ///@}

    /// @name Debugging Features
    ///@{
#ifdef THORIN_ENABLE_CHECKS
    const auto& breakpoints() { return state_.breakpoints; }

    Ref gid2def(u32 gid);     ///< Lookup Def by @p gid.
    void breakpoint(u32 gid); ///< Trigger breakpoint in your debugger when creating Def with Def::gid @p gid.

    World& verify(); ///< Verifies that all externals() and annexes() are Def::is_closed(), if `THORIN_ENABLE_CHECKS`.
#else
    World& verify() { return *this; }
#endif
    ///@}

    /// @name Manage Nodes
    ///@{
    const auto& annexes() const { return move_.annexes; }
    const auto& externals() const { return move_.externals; }
    void make_external(Def* def) {
        assert(!def->is_external());
        assert(def->is_closed());
        def->external_ = true;
        assert_emplace(move_.externals, def->sym(), def);
    }
    void make_internal(Def* def) {
        assert(def->is_external());
        def->external_ = false;
        auto num       = move_.externals.erase(def->sym());
        assert_unused(num == 1);
    }

    Def* external(Sym name) { return mim::lookup(move_.externals, name); } ///< Lookup by @p name.

    /// Lookup annex by Axiom::id.
    template<class Id> const Def* annex(Id id) {
        auto flags = static_cast<flags_t>(id);
        if (auto i = move_.annexes.find(flags); i != move_.annexes.end()) return i->second;
        error("Axiom with ID '{x}' not found; demangled plugin name is '{}'", flags, Annex::demangle(driver(), flags));
    }

    /// Get Axiom from a plugin.
    /// Can be used to get an Axiom without sub-tags.
    /// E.g. use `w.annex<mem::M>();` to get the `%mem.M` Axiom.
    template<annex_without_subs id> const Def* annex() { return annex(Annex::Base<id>); }

    const Def* register_annex(flags_t f, const Def*);
    const Def* register_annex(plugin_t p, tag_t t, sub_t s, const Def* def) {
        return register_annex(p | (flags_t(t) << 8_u64) | flags_t(s), def);
    }
    ///@}

    /// @name Univ, Type, Var, Proxy, Infer
    ///@{
    const Univ* univ() { return data_.univ; }
    Ref uinc(Ref op, level_t offset = 1);
    template<Sort = Sort::Univ> Ref umax(Defs);
    const Type* type(Ref level);
    const Type* type_infer_univ() { return type(mut_infer_univ()); }
    template<level_t level = 0> const Type* type() {
        if constexpr (level == 0)
            return data_.type_0;
        else if constexpr (level == 1)
            return data_.type_1;
        else
            return type(lit_univ(level));
    }
    Ref var(Ref type, Def* mut);
    const Proxy* proxy(Ref type, Defs ops, u32 index, u32 tag) {
        return unify<Proxy>(ops.size(), type, ops, index, tag);
    }

    Infer* mut_infer(Ref type) { return insert<Infer>(1, type); }
    Infer* mut_infer_univ() { return mut_infer(univ()); }
    Infer* mut_infer_type() { return mut_infer(type_infer_univ()); }

    /// Either a value `?:?:.Type ?` or a type `?:.Type ?:.Type ?`.
    Infer* mut_infer_entity() {
        auto t   = type_infer_univ();
        auto res = mut_infer(mut_infer(t));
        assert(this == &res->world());
        return res;
    }
    ///@}

    /// @name Axiom
    ///@{
    const Axiom* axiom(NormalizeFn n, u8 curry, u8 trip, Ref type, plugin_t p, tag_t t, sub_t s) {
        return unify<Axiom>(0, n, curry, trip, type, p, t, s);
    }
    const Axiom* axiom(Ref type, plugin_t p, tag_t t, sub_t s) { return axiom(nullptr, 0, 0, type, p, t, s); }

    /// Builds a fresh Axiom with descending Axiom::sub.
    /// This is useful during testing to come up with some entitiy of a specific type.
    /// It uses the plugin Axiom::Global_Plugin and starts with `0` for Axiom::sub and counts up from there.
    /// The Axiom::tag is set to `0` and the Axiom::normalizer to `nullptr`.
    const Axiom* axiom(NormalizeFn n, u8 curry, u8 trip, Ref type) {
        return axiom(n, curry, trip, type, Annex::Global_Plugin, 0, state_.pod.curr_sub++);
    }
    const Axiom* axiom(Ref type) { return axiom(nullptr, 0, 0, type); } ///< See above.
    ///@}

    /// @name Pi
    ///@{
    // clang-format off
    const Pi* pi(Ref  dom, Ref  codom, bool implicit = false) { return unify<Pi>(2, Pi::infer(dom, codom), dom, codom, implicit); }
    const Pi* pi(Defs dom, Ref  codom, bool implicit = false) { return pi(sigma(dom), codom, implicit); }
    const Pi* pi(Ref  dom, Defs codom, bool implicit = false) { return pi(dom, sigma(codom), implicit); }
    const Pi* pi(Defs dom, Defs codom, bool implicit = false) { return pi(sigma(dom), sigma(codom), implicit); }
    Pi* mut_pi(Ref type, bool implicit = false) { return insert<Pi>(2, type, implicit); }
    // clang-format on
    ///@}

    /// @name Cn
    /// Pi with codom mim::Bot%tom
    ///@{
    // clang-format off
    const Pi* cn(                                           ) { return cn(sigma(   ),                   false); }
    const Pi* cn(Ref  dom,             bool implicit = false) { return pi(      dom ,    type_bot(), implicit); }
    const Pi* cn(Defs dom,             bool implicit = false) { return cn(sigma(dom),                implicit); }
    const Pi* fn(Ref  dom, Ref  codom, bool implicit = false) { return cn({     dom ,    cn(codom)}, implicit); }
    const Pi* fn(Defs dom, Ref  codom, bool implicit = false) { return fn(sigma(dom),       codom,   implicit); }
    const Pi* fn(Ref  dom, Defs codom, bool implicit = false) { return fn(      dom , sigma(codom),  implicit); }
    const Pi* fn(Defs dom, Defs codom, bool implicit = false) { return fn(sigma(dom), sigma(codom),  implicit); }
    // clang-format on
    ///@}

    /// @name Lam
    ///@{
    Ref filter(Lam::Filter filter) {
        if (auto b = std::get_if<bool>(&filter)) return lit_bool(*b);
        return std::get<const Def*>(filter);
    }
    const Lam* lam(const Pi* pi, Lam::Filter f, Ref body) { return unify<Lam>(2, pi, filter(f), body); }
    Lam* mut_lam(const Pi* pi) { return insert<Lam>(2, pi); }
    // clang-format off
    const Lam* con(Ref  dom,             Lam::Filter f, Ref body) { return unify<Lam>(2, cn(dom        ), filter(f), body); }
    const Lam* con(Defs dom,             Lam::Filter f, Ref body) { return unify<Lam>(2, cn(dom        ), filter(f), body); }
    const Lam* lam(Ref  dom, Ref  codom, Lam::Filter f, Ref body) { return unify<Lam>(2, pi(dom,  codom), filter(f), body); }
    const Lam* lam(Defs dom, Ref  codom, Lam::Filter f, Ref body) { return unify<Lam>(2, pi(dom,  codom), filter(f), body); }
    const Lam* lam(Ref  dom, Defs codom, Lam::Filter f, Ref body) { return unify<Lam>(2, pi(dom,  codom), filter(f), body); }
    const Lam* lam(Defs dom, Defs codom, Lam::Filter f, Ref body) { return unify<Lam>(2, pi(dom,  codom), filter(f), body); }
    const Lam* fun(Ref  dom, Ref  codom, Lam::Filter f, Ref body) { return unify<Lam>(2, fn(dom , codom), filter(f), body); }
    const Lam* fun(Defs dom, Ref  codom, Lam::Filter f, Ref body) { return unify<Lam>(2, fn(dom,  codom), filter(f), body); }
    const Lam* fun(Ref  dom, Defs codom, Lam::Filter f, Ref body) { return unify<Lam>(2, fn(dom,  codom), filter(f), body); }
    const Lam* fun(Defs dom, Defs codom, Lam::Filter f, Ref body) { return unify<Lam>(2, fn(dom,  codom), filter(f), body); }
    Lam* mut_con(Ref  dom            ) { return insert<Lam>(2, cn(dom       )); }
    Lam* mut_con(Defs dom            ) { return insert<Lam>(2, cn(dom       )); }
    Lam* mut_lam(Ref  dom, Ref  codom) { return insert<Lam>(2, pi(dom, codom)); }
    Lam* mut_lam(Defs dom, Ref  codom) { return insert<Lam>(2, pi(dom, codom)); }
    Lam* mut_lam(Ref  dom, Defs codom) { return insert<Lam>(2, pi(dom, codom)); }
    Lam* mut_lam(Defs dom, Defs codom) { return insert<Lam>(2, pi(dom, codom)); }
    Lam* mut_fun(Ref  dom, Ref  codom) { return insert<Lam>(2, fn(dom, codom)); }
    Lam* mut_fun(Defs dom, Ref  codom) { return insert<Lam>(2, fn(dom, codom)); }
    Lam* mut_fun(Ref  dom, Defs codom) { return insert<Lam>(2, fn(dom, codom)); }
    Lam* mut_fun(Defs dom, Defs codom) { return insert<Lam>(2, fn(dom, codom)); }
    // clang-format on
    Lam* exit() { return data_.exit; } ///< Used as a dummy exit node within Scope.
    ///@}

    /// @name App
    ///@{
    Ref app(Ref callee, Ref arg);
    Ref app(Ref callee, Defs args) { return app(callee, tuple(args)); }
    template<bool Normalize = false> Ref raw_app(Ref type, Ref callee, Ref arg);
    template<bool Normalize = false> Ref raw_app(Ref type, Ref callee, Defs args) {
        return raw_app<Normalize>(type, callee, tuple(args));
    }
    ///@}

    /// @name Sigma
    ///@{
    Sigma* mut_sigma(Ref type, size_t size) { return insert<Sigma>(size, type, size); }
    /// A *mut*able Sigma of type @p level.
    template<level_t level = 0> Sigma* mut_sigma(size_t size) { return mut_sigma(type<level>(), size); }
    Ref sigma(Defs ops);
    const Sigma* sigma() { return data_.sigma; } ///< The unit type within Type 0.
    ///@}

    /// @name Arr
    ///@{
    Arr* mut_arr(Ref type) { return insert<Arr>(2, type); }
    template<level_t level = 0> Arr* mut_arr() { return mut_arr(type<level>()); }
    Ref arr(Ref shape, Ref body);
    Ref arr(Defs shape, Ref body);
    Ref arr(u64 n, Ref body) { return arr(lit_nat(n), body); }
    Ref arr(View<u64> shape, Ref body) {
        return arr(DefVec(shape.size(), [&](size_t i) { return lit_nat(shape[i]); }), body);
    }
    Ref arr_unsafe(Ref body) { return arr(top_nat(), body); }
    ///@}

    /// @name Tuple
    ///@{
    Ref tuple(Defs ops);
    /// Ascribes @p type to this tuple - needed for dependently typed and mutable Sigma%s.
    Ref tuple(Ref type, Defs ops);
    const Tuple* tuple() { return data_.tuple; } ///< the unit value of type `[]`
    Ref tuple(Sym sym);                          ///< Converts @p sym to a tuple of type '«n; I8»'.
    ///@}

    /// @name Pack
    ///@{
    Pack* mut_pack(Ref type) { return insert<Pack>(1, type); }
    Ref pack(Ref arity, Ref body);
    Ref pack(Defs shape, Ref body);
    Ref pack(u64 n, Ref body) { return pack(lit_nat(n), body); }
    Ref pack(View<u64> shape, Ref body) {
        return pack(DefVec(shape.size(), [&](auto i) { return lit_nat(shape[i]); }), body);
    }
    ///@}

    /// @name Extract
    /// @see core::extract_unsafe
    ///@{
    Ref extract(Ref d, Ref i);
    Ref extract(Ref d, u64 a, u64 i) { return extract(d, lit_idx(a, i)); }
    Ref extract(Ref d, u64 i) { return extract(d, d->as_lit_arity(), i); }

    /// Builds `(f, t)#cond`.
    /// @note Expects @p cond as first, @p t as second, and @p f as third argument.
    Ref select(Ref cond, Ref t, Ref f) { return extract(tuple({f, t}), cond); }
    ///@}

    /// @name Insert
    /// @see core::insert_unsafe
    ///@{
    Ref insert(Ref d, Ref i, Ref val);
    Ref insert(Ref d, u64 a, u64 i, Ref val) { return insert(d, lit_idx(a, i), val); }
    Ref insert(Ref d, u64 i, Ref val) { return insert(d, d->as_lit_arity(), i, val); }
    ///@}

    /// @name Lit
    ///@{
    const Lit* lit(Ref type, u64 val);
    const Lit* lit_univ(u64 level) { return lit(univ(), level); }
    const Lit* lit_univ_0() { return data_.lit_univ_0; }
    const Lit* lit_univ_1() { return data_.lit_univ_1; }
    const Lit* lit_nat(nat_t a) { return lit(type_nat(), a); }
    const Lit* lit_nat_0() { return data_.lit_nat_0; }
    const Lit* lit_nat_1() { return data_.lit_nat_1; }
    const Lit* lit_nat_max() { return data_.lit_nat_max; }
    const Lit* lit_0_1() { return data_.lit_0_1; }
    // clang-format off
    const Lit* lit_i1()  { return lit_nat(Idx::bitwidth2size( 1)); };
    const Lit* lit_i8()  { return lit_nat(Idx::bitwidth2size( 8)); };
    const Lit* lit_i16() { return lit_nat(Idx::bitwidth2size(16)); };
    const Lit* lit_i32() { return lit_nat(Idx::bitwidth2size(32)); };
    const Lit* lit_i64() { return lit_nat(Idx::bitwidth2size(64)); };
    /// Constructs a Lit of type Idx of size @p size.
    /// @note `size = 0` means `2^64`.
    const Lit* lit_idx(nat_t size, u64 val) { return lit(type_idx(size), val); }

    template<class I> const Lit* lit_idx(I val) {
        static_assert(std::is_integral<I>());
        return lit_idx(Idx::bitwidth2size(sizeof(I) * 8), val);
    }

    /// Constructs a Lit @p of type Idx of size $2^width$.
    /// `val = 64` will be automatically converted to size `0` - the encoding for $2^64$.
    const Lit* lit_int(nat_t width, u64 val) { return lit_idx(Idx::bitwidth2size(width), val); }
    const Lit* lit_i1 (bool val) { return lit_int( 1, u64(val)); }
    const Lit* lit_i2 (u8   val) { return lit_int( 2, u64(val)); }
    const Lit* lit_i4 (u8   val) { return lit_int( 4, u64(val)); }
    const Lit* lit_i8 (u8   val) { return lit_int( 8, u64(val)); }
    const Lit* lit_i16(u16  val) { return lit_int(16, u64(val)); }
    const Lit* lit_i32(u32  val) { return lit_int(32, u64(val)); }
    const Lit* lit_i64(u64  val) { return lit_int(64, u64(val)); }
    // clang-format on

    /// Constructs a Lit of type Idx of size @p mod.
    /// The value @p val will be adjusted modulo @p mod.
    /// @note `mod == 0` is the special case for $2^64$ and no modulo will be performed on @p val.
    const Lit* lit_idx_mod(nat_t mod, u64 val) { return lit_idx(mod, mod == 0 ? val : (val % mod)); }

    const Lit* lit_bool(bool val) { return data_.lit_bool[size_t(val)]; }
    const Lit* lit_ff() { return data_.lit_bool[0]; }
    const Lit* lit_tt() { return data_.lit_bool[1]; }
    // clang-format off
    ///@}

    /// @name Lattice
    ///@{
    template<bool Up>
    Ref ext(Ref type);
    Ref bot(Ref type) { return ext<false>(type); }
    Ref top(Ref type) { return ext<true>(type); }
    Ref type_bot() { return data_.type_bot; }
    Ref type_top() { return data_.type_top; }
    Ref top_nat() { return data_.top_nat; }
    template<bool Up> TBound<Up>* mut_bound(Ref type, size_t size) { return insert<TBound<Up>>(size, type, size); }
    /// A *mut*able Bound of Type @p l%evel.
    template<bool Up, level_t l = 0> TBound<Up>* mut_bound(size_t size) { return mut_bound<Up>(type<l>(), size); }
    template<bool Up> Ref bound(Defs ops);
    Join* mut_join(Ref type, size_t size) { return mut_bound<true>(type, size); }
    Meet* mut_meet(Ref type, size_t size) { return mut_bound<false>(type, size); }
    template<level_t l = 0> Join* mut_join(size_t size) { return mut_join(type<l>(), size); }
    template<level_t l = 0> Meet* mut_meet(size_t size) { return mut_meet(type<l>(), size); }
    Ref join(Defs ops) { return bound<true>(ops); }
    Ref meet(Defs ops) { return bound<false>(ops); }
    Ref ac(Ref type, Defs ops);
    /// Infers the type using an *immutable* Meet.
    Ref ac(Defs ops);
    Ref vel(Ref type, Ref value);
    Ref pick(Ref type, Ref value);
    Ref test(Ref value, Ref probe, Ref match, Ref clash);
    Ref singleton(Ref inner_type);
    ///@}

    /// @name Globals
    /// @deprecated { will be removed }
    ///@{
    Global* global(Ref type, bool is_mutable = true) { return insert<Global>(1, type, is_mutable); }
    ///@}
    // clang-format on

    /// @name Types
    ///@{
    const Nat* type_nat() { return data_.type_nat; }
    const Idx* type_idx() { return data_.type_idx; }
    /// @note `size = 0` means `2^64`.
    Ref type_idx(Ref size) { return app(type_idx(), size); }
    /// @note `size = 0` means `2^64`.
    Ref type_idx(nat_t size) { return type_idx(lit_nat(size)); }

    /// Constructs a type Idx of size $2^width$.
    /// `width = 64` will be automatically converted to size `0` - the encoding for $2^64$.
    Ref type_int(nat_t width) { return type_idx(lit_nat(Idx::bitwidth2size(width))); }
    // clang-format off
    Ref type_bool() { return data_.type_bool; }
    Ref type_i1()   { return data_.type_bool; }
    Ref type_i2()   { return type_int( 2);    };
    Ref type_i4()   { return type_int( 4);    };
    Ref type_i8()   { return type_int( 8);    };
    Ref type_i16()  { return type_int(16);    };
    Ref type_i32()  { return type_int(32);    };
    Ref type_i64()  { return type_int(64);    };
    // clang-format on
    ///@}

    /// @name Cope with implicit Arguments
    ///@{

    /// Places Infer arguments as demanded by Pi::implicit and then apps @p arg.
    Ref iapp(Ref callee, Ref arg);
    Ref iapp(Ref callee, Defs args) { return iapp(callee, tuple(args)); }
    Ref iapp(Ref callee, nat_t arg) { return iapp(callee, lit_nat(arg)); }
    template<class E>
    Ref iapp(Ref callee, E arg) requires std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, nat_t>
    {
        return iapp(callee, lit_nat((nat_t)arg));
    }

    /// @name Vars & Muts
    ///@{
    /// Manges sets of Vars and Muts.
    [[nodiscard]] Vars vars(const Var* var) { return move_.vars.singleton(var); }
    [[nodiscard]] Muts muts(Def* mut) { return move_.muts.singleton(mut); }
    [[nodiscard]] Vars merge(Vars a, Vars b) { return move_.vars.merge(a, b); }
    [[nodiscard]] Muts merge(Muts a, Muts b) { return move_.muts.merge(a, b); }
    [[nodiscard]] Vars insert(Vars vars, const Var* var) { return move_.vars.insert(vars, var); }
    [[nodiscard]] Muts insert(Muts muts, Def* mut) { return move_.muts.insert(muts, mut); }
    [[nodiscard]] Vars erase(Vars vars, const Var* var) { return move_.vars.erase(vars, var); }
    [[nodiscard]] Muts erase(Muts muts, Def* mut) { return move_.muts.erase(muts, mut); }
    [[nodiscard]] bool has_intersection(Vars v1, Vars v2) { return move_.vars.has_intersection(v1, v2); }
    [[nodiscard]] bool has_intersection(Muts m1, Muts m2) { return move_.muts.has_intersection(m1, m2); }
    ///@}

    // clang-format off
    template<class Id, class... Args> const Def* call(Id id, Args&&... args) { return call_(annex(id),   std::forward<Args>(args)...); }
    template<class Id, class... Args> const Def* call(       Args&&... args) { return call_(annex<Id>(), std::forward<Args>(args)...); }
    template<class T, class... Args> const Def* call_(Ref callee, T arg, Args&&... args) { return call_(iapp(callee, arg), std::forward<Args>(args)...); }
    template<class T> const Def* call_(Ref callee, T arg) { return iapp(callee, arg); }
    // clang-format on
    ///@}

    /// @name Helpers
    ///@{
    Ref iinfer(Ref def) { return Idx::size(def->type()); }
    ///@}

    /// @name dump/log
    ///@{
    Log& log();
    void dummy() {}

    void dump(std::ostream& os);  ///< Dump to @p os.
    void dump();                  ///< Dump to `std::cout`.
    void debug_dump();            ///< Dump in Debug build if World::log::level is Log::Level::Debug.
    void write(const char* file); ///< Write to a file named @p file.
    void write();                 ///< Same above but file name defaults to World::name.
    ///@}

    /// @name dot
    ///@{
    /// Dumps DOT to @p os.
    /// @param os Output stream
    /// @param annexes If `true`, include all annexes - even if unused.
    /// @param types Follow type dependencies?
    void dot(std::ostream& os, bool annexes = false, bool types = false) const;
    /// Same as above but write to @p file or `std::cout` if @p file is `nullptr`.
    void dot(const char* file = nullptr, bool annexes = false, bool types = false) const;
    ///@}

private:
    /// @name Put into Sea of Nodes
    ///@{
    template<class T, class... Args> const T* unify(size_t num_ops, Args&&... args) {
        auto state = move_.arena.state();
        auto def   = allocate<T>(num_ops, std::forward<Args&&>(args)...);
        if (auto loc = get_loc()) def->set(loc);
        assert(!def->isa_mut());
#ifdef THORIN_ENABLE_CHECKS
        if (flags().trace_gids) outln("{}: {} - {}", def->node_name(), def->gid(), def->flags());
        if (flags().reeval_breakpoints && breakpoints().contains(def->gid())) fe::breakpoint();
#endif
        if (is_frozen()) {
            --state_.pod.curr_gid;
            auto i = move_.defs.find(def);
            deallocate<T>(state, def);
            if (i != move_.defs.end()) return static_cast<const T*>(*i);
            return nullptr;
        }

        if (auto [i, ins] = move_.defs.emplace(def); !ins) {
            deallocate<T>(state, def);
            return static_cast<const T*>(*i);
        }
#ifdef THORIN_ENABLE_CHECKS
        if (!flags().reeval_breakpoints && breakpoints().contains(def->gid())) fe::breakpoint();
#endif
        def->finalize();
        return def;
    }

    template<class T> void deallocate(fe::Arena::State state, const T* ptr) {
        ptr->~T();
        move_.arena.deallocate(state);
    }

    template<class T, class... Args> T* insert(size_t num_ops, Args&&... args) {
        auto def = allocate<T>(num_ops, std::forward<Args&&>(args)...);
        if (auto loc = get_loc()) def->set(loc);
#ifdef THORIN_ENABLE_CHECKS
        if (flags().trace_gids) outln("{}: {} - {}", def->node_name(), def->gid(), def->flags());
        if (breakpoints().contains(def->gid())) fe::breakpoint();
#endif
        assert_emplace(move_.defs, def);
        return def;
    }

#if (!defined(_MSC_VER) && defined(NDEBUG))
    struct Lock {
        Lock() { assert((guard_ = !guard_) && "you are not allowed to recursively invoke allocate"); }
        ~Lock() { guard_ = !guard_; }
        static bool guard_;
    };
#else
    struct Lock {
        ~Lock() {}
    };
#endif

    template<class T, class... Args> T* allocate(size_t num_ops, Args&&... args) {
        static_assert(sizeof(Def) == sizeof(T),
                      "you are not allowed to introduce any additional data in subclasses of Def");
        Lock lock;
        move_.arena.align(alignof(T));
        size_t num_bytes = sizeof(Def) + sizeof(uintptr_t) * num_ops;
        auto ptr         = move_.arena.allocate(num_bytes);
        auto res         = new (ptr) T(std::forward<Args&&>(args)...);
        assert(res->num_ops() == num_ops);
        return res;
    }
    ///@}

    Driver* driver_;
    State state_;

    struct SeaHash {
        size_t operator()(const Def* def) const { return def->hash(); }
    };

    struct SeaEq {
        bool operator()(const Def* d1, const Def* d2) const { return d1->equal(d2); }
    };

    struct Move {
        absl::btree_map<flags_t, const Def*> annexes;
        absl::btree_map<Sym, Def*> externals;
        fe::Arena arena;
        absl::flat_hash_set<const Def*, SeaHash, SeaEq> defs;
        Pool<const Var*> vars;
        Pool<Def*> muts;
        DefDefMap<DefVec> cache;

        friend void swap(Move& m1, Move& m2) noexcept {
            using std::swap;
            // clang-format off
            swap(m1.annexes,    m2.annexes);
            swap(m1.externals,  m2.externals);
            swap(m1.arena,      m2.arena);
            swap(m1.defs,       m2.defs);
            swap(m1.vars,       m2.vars);
            swap(m1.muts,       m2.muts);
            swap(m1.cache,      m2.cache);
            // clang-format on
        }
    } move_;

    struct {
        const Univ* univ;
        const Type* type_0;
        const Type* type_1;
        const Bot* type_bot;
        const Top* type_top;
        const Def* type_bool;
        const Top* top_nat;
        const Sigma* sigma;
        const Tuple* tuple;
        const Nat* type_nat;
        const Idx* type_idx;
        const Def* table_id;
        const Def* table_not;
        const Lit* lit_univ_0;
        const Lit* lit_univ_1;
        const Lit* lit_nat_0;
        const Lit* lit_nat_1;
        const Lit* lit_nat_max;
        const Lit* lit_0_1;
        std::array<const Lit*, 2> lit_bool;
        Lam* exit;
    } data_;

    friend void swap(World& w1, World& w2) noexcept {
        using std::swap;
        // clang-format off
        swap(w1.state_, w2.state_);
        swap(w1.data_,  w2.data_ );
        swap(w1.move_,  w2.move_ );
        // clang-format on

        swap(w1.data_.univ->world_, w2.data_.univ->world_);
        assert(&w1.univ()->world() == &w1);
        assert(&w2.univ()->world() == &w2);
    }

    friend DefVec Def::reduce(const Def*);
};

} // namespace mim
