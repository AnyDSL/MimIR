#pragma once

#include <sstream>
#include <string>
#include <string_view>

#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>

#include "thorin/axiom.h"
#include "thorin/check.h"
#include "thorin/config.h"
#include "thorin/flags.h"
#include "thorin/lattice.h"
#include "thorin/tuple.h"

#include "thorin/util/hash.h"
#include "thorin/util/log.h"
#include "thorin/util/sym.h"

namespace thorin {
class Checker;
class Driver;

using Breakpoints = absl::flat_hash_set<uint32_t>;

/// The World represents the whole program and manages creation of Thorin nodes (Def%s).
/// *Structural* Def%s are hashed into an internal HashSet.
/// The getters just calculate a hash and lookup the Def, if it is already present, or create a new one otherwise.
/// This corresponds to value numbering.
///
/// You can create several worlds.
/// All worlds are completely independent from each other.
///
/// Note that types are also just Def%s and will be hashed as well.
class World {
public:
    /// @name state
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

        friend void swap(State& s1, State& s2) {
            using std::swap;
            assert((!s1.pod.loc || !s2.pod.loc) && "Why is emit_loc() still set?");
            // clang-format off
            swap(s1.pod,                s2.pod);
            // clang-format on
        }
    };

    /// @name c'tor and d'tor
    ///@{
    World& operator=(const World&) = delete;

    /// Inherits the @p state into the new World.
    explicit World(Driver*);
    World(Driver*, const State&);
    World(World&& other)
        : World(&other.driver(), other.state()) {
        swap(*this, other);
    }
    World inherit() { return World(&driver(), state()); }
    ~World();
    ///@}

    /// @name misc getters/setters
    ///@{
    const State& state() const { return state_; }

    const Driver& driver() const { return *driver_; }
    Driver& driver() { return *driver_; }

    Sym name() const { return state_.pod.name; }
    void set(Sym name) { state_.pod.name = name; }

    /// Manage global identifier - a unique number for each Def.
    u32 curr_gid() const { return state_.pod.curr_gid; }
    u32 next_gid() { return ++state_.pod.curr_gid; }

    /// Retrive compile Flags.
    const Flags& flags() const;
    Flags& flags();

    Checker& checker() {
        assert(&move_.checker->world() == this);
        return *move_.checker;
    }

    Loc& emit_loc() { return state_.pod.loc; }
    ///@}

    /// @name Sym
    ///@{
    Sym sym(std::string_view);
    Sym sym(const char*);
    Sym sym(std::string);
    ///@}

    /// @name freeze
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

#if THORIN_ENABLE_CHECKS
    /// @name debugging features
    ///@{
    Breakpoints& breakpoints();
    void breakpoint(size_t number);
    Ref gid2def(u32 gid);
    ///@}
#endif

    /// @name manage nodes
    ///@{
    const auto& axioms() const { return move_.axioms; }
    const auto& externals() const { return move_.externals; }
    bool empty() { return move_.externals.empty(); }
    void make_external(Def* def) {
        assert(!def->external_);
        def->external_ = true;
        auto [i, ins]  = move_.externals.emplace(def->sym(), def);
        assert(ins);
    }
    void make_internal(Def* def) {
        assert(def->external_);
        def->external_ = false;
        auto num       = move_.externals.erase(def->sym());
        assert(num == 1);
    }
    Def* lookup(Sym name) {
        auto i = move_.externals.find(name);
        return i != move_.externals.end() ? i->second : nullptr;
    }
    ///@}

    /// @name Univ, Type, Var, Proxy, Infer
    ///@{
    const Univ* univ() { return data_.univ; }
    Ref uinc(Ref op, level_t offset = 1);
    template<Sort = Sort::Univ>
    Ref umax(DefArray);
    const Type* type(Ref level);
    const Type* type_infer_univ() { return type(nom_infer_univ()); }
    template<level_t level = 0>
    const Type* type() {
        if constexpr (level == 0)
            return data_.type_0;
        else if constexpr (level == 1)
            return data_.type_1;
        else
            return type(lit_univ(level));
    }
    const Var* var(Ref type, Def* nom) { return unify<Var>(1, type, nom); }
    const Proxy* proxy(Ref type, Defs ops, u32 index, u32 tag) {
        return unify<Proxy>(ops.size(), type, ops, index, tag);
    }

    Infer* nom_infer(Ref type) { return insert<Infer>(1, type); }
    Infer* nom_infer_univ() { return nom_infer(univ()); }
    Infer* nom_infer_type() { return nom_infer(type_infer_univ()); }

    /// Either a value `?:?:.Type ?` or a type `?:.Type ?:.Type ?`.
    Infer* nom_infer_entity() {
        auto t   = type_infer_univ();
        auto res = nom_infer(nom_infer(t));
        assert(this == &res->world());
        return res;
    }
    ///@}

    /// @name Axiom
    ///@{
    const Axiom* axiom(Def::NormalizeFn n, u8 curry, u8 trip, Ref type, dialect_t d, tag_t t, sub_t s) {
        auto ax                          = unify<Axiom>(0, n, curry, trip, type, d, t, s);
        return move_.axioms[ax->flags()] = ax;
    }
    const Axiom* axiom(Ref type, dialect_t d, tag_t t, sub_t s) { return axiom(nullptr, 0, 0, type, d, t, s); }

    /// Builds a fresh Axiom with descending Axiom::sub.
    /// This is useful during testing to come up with some entitiy of a specific type.
    /// It uses the dialect Axiom::Global_Dialect and starts with `0` for Axiom::sub and counts up from there.
    /// The Axiom::tag is set to `0` and the Axiom::normalizer to `nullptr`.
    const Axiom* axiom(Def::NormalizeFn n, u8 curry, u8 trip, Ref type) {
        return axiom(n, curry, trip, type, Axiom::Global_Dialect, 0, state_.pod.curr_sub++);
    }
    const Axiom* axiom(Ref type) { return axiom(nullptr, 0, 0, type); } ///< See above.

    /// Get Axiom from a dialect.
    /// Use this to get an Axiom via Axiom::id.
    template<class Id>
    const Axiom* ax(Id id) {
        u64 flags = static_cast<u64>(id);
        if (auto i = move_.axioms.find(flags); i != move_.axioms.end()) return i->second;
        err("Axiom with ID '{}' not found; demangled dialect name is '{}'", flags, Axiom::demangle(*this, flags));
    }

    /// Get Axiom from a dialect.
    /// Can be used to get an Axiom without sub-tags.
    /// E.g. use `w.ax<mem::M>();` to get the `%mem.M` Axiom.
    template<axiom_without_subs id>
    const Axiom* ax() {
        return ax(Axiom::Base<id>);
    }
    ///@}

    /// @name Pi
    ///@{
    const Pi* pi(Ref dom, Ref codom, bool implicit = false) {
        return unify<Pi>(2, Pi::infer(dom, codom), dom, codom, implicit);
    }
    const Pi* pi(Defs dom, Ref codom, bool implicit = false) { return pi(sigma(dom), codom, implicit); }
    Pi* nom_pi(Ref type, bool implicit = false) { return insert<Pi>(2, type, implicit); }
    ///@}

    /// @name Cn (Pi with codom Bot)
    ///@{
    const Pi* cn() { return cn(sigma()); }
    const Pi* cn(Ref dom) { return pi(dom, type_bot()); }
    const Pi* cn(Defs doms) { return cn(sigma(doms)); }
    ///@}

    /// @name Lam
    ///@{
    Lam* nom_lam(const Pi* cn) { return insert<Lam>(2, cn); }
    const Lam* lam(const Pi* pi, Ref filter, Ref body) { return unify<Lam>(2, pi, filter, body); }
    const Lam* lam(const Pi* pi, Ref body) { return lam(pi, lit_tt(), body); }
    Lam* exit() { return data_.exit; } ///< Used as a dummy exit node within Scope.
    ///@}

    /// @name App
    ///@{
    Ref app(Ref callee, Ref arg);
    Ref app(Ref callee, Defs args) { return app(callee, tuple(args)); }
    template<bool Normalize = false>
    Ref raw_app(Ref type, Ref callee, Ref arg);
    template<bool Normalize = false>
    Ref raw_app(Ref type, Ref callee, Defs args) {
        return raw_app<Normalize>(type, callee, tuple(args));
    }
    ///@}

    /// @name Sigma
    ///@{
    Sigma* nom_sigma(Ref type, size_t size) { return insert<Sigma>(size, type, size); }
    /// A *nom*inal Sigma of type @p level.
    template<level_t level = 0>
    Sigma* nom_sigma(size_t size) {
        return nom_sigma(type<level>(), size);
    }
    Ref sigma(Defs ops);
    const Sigma* sigma() { return data_.sigma; } ///< The unit type within Type 0.
    ///@}

    /// @name Arr
    ///@{
    Arr* nom_arr(Ref type) { return insert<Arr>(2, type); }
    template<level_t level = 0>
    Arr* nom_arr() {
        return nom_arr(type<level>());
    }
    Ref arr(Ref shape, Ref body);
    Ref arr(Defs shape, Ref body);
    Ref arr(u64 n, Ref body) { return arr(lit_nat(n), body); }
    Ref arr(Span<u64> shape, Ref body) {
        return arr(DefArray(shape.size(), [&](size_t i) { return lit_nat(shape[i]); }), body);
    }
    Ref arr_unsafe(Ref body) { return arr(top_nat(), body); }
    ///@}

    /// @name Tuple
    ///@{
    Ref tuple(Defs ops);
    /// Ascribes @p type to this tuple - needed for dependently typed and nominal Sigma%s.
    Ref tuple(Ref type, Defs ops);
    const Tuple* tuple() { return data_.tuple; } ///< the unit value of type `[]`
    ///@}

    /// @name Pack
    ///@{
    Pack* nom_pack(Ref type) { return insert<Pack>(1, type); }
    Ref pack(Ref arity, Ref body);
    Ref pack(Defs shape, Ref body);
    Ref pack(u64 n, Ref body) { return pack(lit_nat(n), body); }
    Ref pack(Span<u64> shape, Ref body) {
        return pack(DefArray(shape.size(), [&](auto i) { return lit_nat(shape[i]); }), body);
    }
    ///@}

    /// @name Extract
    /// @sa core::extract_unsafe
    ///@{
    Ref extract(Ref d, Ref i);
    Ref extract(Ref d, u64 a, u64 i) { return extract(d, lit_idx(a, i)); }
    Ref extract(Ref d, u64 i) { return extract(d, d->as_lit_arity(), i); }

    /// Builds `(f, t)cond`.
    /// **Note** that select expects @p t as first argument and @p f as second one.
    Ref select(Ref t, Ref f, Ref cond) { return extract(tuple({f, t}), cond); }
    ///@}

    /// @name Insert
    /// @sa core::insert_unsafe
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
    /// Constructs a Lit of type Idx of size @p size.
    /// @note `size = 0` means `2^64`.
    const Lit* lit_idx(nat_t size, u64 val) { return lit(type_idx(size), val); }

    template<class I>
    const Lit* lit_idx(I val) {
        static_assert(std::is_integral<I>());
        return lit_idx(Idx::bitwidth2size(sizeof(I) * 8), val);
    }

    /// Constructs a Lit @p of type Idx of size $2^width$.
    /// `val = 64` will be automatically converted to size `0` - the encoding for $2^64$.
    const Lit* lit_int(nat_t width, u64 val) { return lit_idx(Idx::bitwidth2size(width), val); }

    /// Constructs a Lit of type Idx of size @p mod.
    /// The value @p val will be adjusted modulo @p mod.
    /// @note `mod == 0` is the special case for $2^64$ and no modulo will be performed on @p val.
    const Lit* lit_idx_mod(nat_t mod, u64 val) { return lit_idx(mod, mod == 0 ? val : (val % mod)); }

    const Lit* lit_bool(bool val) { return data_.lit_bool[size_t(val)]; }
    const Lit* lit_ff() { return data_.lit_bool[0]; }
    const Lit* lit_tt() { return data_.lit_bool[1]; }
    // clang-format off
    ///@}

    /// @name lattice
    ///@{
    template<bool Up>
    Ref ext(Ref type);
    Ref bot(Ref type) { return ext<false>(type); }
    Ref top(Ref type) { return ext<true>(type); }
    Ref type_bot() { return data_.type_bot; }
    Ref top_nat() { return data_.top_nat; }
    template<bool Up> TBound<Up>* nom_bound(Ref type, size_t size) { return insert<TBound<Up>>(size, type, size); }
    /// A *nom*inal Bound of Type @p l%evel.
    template<bool Up, level_t l = 0> TBound<Up>* nom_bound(size_t size) { return nom_bound<Up>(type<l>(), size); }
    template<bool Up> Ref bound(Defs ops);
    Join* nom_join(Ref type, size_t size) { return nom_bound<true>(type, size); }
    Meet* nom_meet(Ref type, size_t size) { return nom_bound<false>(type, size); }
    template<level_t l = 0> Join* nom_join(size_t size) { return nom_join(type<l>(), size); }
    template<level_t l = 0> Meet* nom_meet(size_t size) { return nom_meet(type<l>(), size); }
    Ref join(Defs ops) { return bound<true>(ops); }
    Ref meet(Defs ops) { return bound<false>(ops); }
    Ref ac(Ref type, Defs ops);
    /// Infers the type using a *structural* Meet.
    Ref ac(Defs ops);
    Ref vel(Ref type, Ref value);
    Ref pick(Ref type, Ref value);
    Ref test(Ref value, Ref probe, Ref match, Ref clash);
    Ref singleton(Ref inner_type);
    ///@}

    /// @name globals -- depdrecated; will be removed
    ///@{
    Global* global(Ref type, bool is_mutable = true) { return insert<Global>(1, type, is_mutable); }
    ///@}
    // clang-format on

    /// @name types
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
    Ref type_bool() { return data_.type_bool; }
    ///@}

    /// @name cope with implicit arguments
    ///@{

    /// Places Infer arguments as demanded by @p debug.meta and then apps @p arg.
    Ref iapp(Ref callee, Ref arg);
    Ref iapp(Ref callee, Defs args) { return iapp(callee, tuple(args)); }
    Ref iapp(Ref callee, nat_t arg) { return iapp(callee, lit_nat(arg)); }

    // clang-format off
    template<class Id, class... Args> const Def* call(Id id, Args&&... args) { return call_(ax(id),   std::forward<Args>(args)...); }
    template<class Id, class... Args> const Def* call(       Args&&... args) { return call_(ax<Id>(), std::forward<Args>(args)...); }
    template<class T, class... Args> const Def* call_(Ref callee, T arg, Args&&... args) { return call_(iapp(callee, arg), std::forward<Args>(args)...); }
    template<class T> const Def* call_(Ref callee, T arg) { return iapp(callee, arg); }
    // clang-format on
    ///@}

    /// @name helpers
    ///@{
    Ref iinfer(Ref def) { return Idx::size(def->type()); }
    ///@}

    /// @name dumping/logging
    ///@{
    const Log& log() const;
    Log& log();
    void dummy() {}

    void dump(std::ostream& os);  ///< Dump to @p os.
    void dump();                  ///< Dump to `std::cout`.
    void debug_dump();            ///< Dump in Debug build if World::log::level is Log::Level::Debug.
    void write(const char* file); ///< Write to a file named @p file.
    void write();                 ///< Same above but file name defaults to World::name.
    ///@}

private:
    /// @name put into sea of nodes
    ///@{
    template<class T, class... Args>
    const T* unify(size_t num_ops, Args&&... args) {
        auto def = arena_.allocate<T>(num_ops, std::forward<Args&&>(args)...);
        if (auto loc = emit_loc()) def->set(loc);
        assert(!def->isa_nom());
#if THORIN_ENABLE_CHECKS
        if (flags().trace_gids) outln("{}: {} - {}", def->node_name(), def->gid(), def->flags());
        if (flags().reeval_breakpoints && breakpoints().contains(def->gid())) thorin::breakpoint();
#endif
        if (is_frozen()) {
            --state_.pod.curr_gid;
            auto i = move_.defs.find(def);
            arena_.deallocate<T>(def);
            if (i != move_.defs.end()) return static_cast<const T*>(*i);
            return nullptr;
        }

        if (auto [i, ins] = move_.defs.emplace(def); !ins) {
            arena_.deallocate<T>(def);
            return static_cast<const T*>(*i);
        }
#if THORIN_ENABLE_CHECKS
        if (!flags().reeval_breakpoints && breakpoints().contains(def->gid())) thorin::breakpoint();
#endif
        def->finalize();
        return def;
    }

    template<class T, class... Args>
    T* insert(size_t num_ops, Args&&... args) {
        auto def = arena_.allocate<T>(num_ops, std::forward<Args&&>(args)...);
        if (auto loc = emit_loc()) def->set(loc);
#if THORIN_ENABLE_CHECKS
        if (flags().trace_gids) outln("{}: {} - {}", def->node_name(), def->gid(), def->flags());
        if (breakpoints().contains(def->gid())) thorin::breakpoint();
#endif
        auto [_, ins] = move_.defs.emplace(def);
        assert_unused(ins);
        return def;
    }
    ///@}

    Driver* driver_;
    State state_;

    class Arena {
    public:
        Arena()
            : root_(new Zone) // don't use 'new Zone()' - we keep the allocated Zone uninitialized
            , curr_(root_.get()) {}

        struct Zone {
            static const size_t Size = 1024 * 1024 - sizeof(std::unique_ptr<int>); // 1MB - sizeof(next)
            char buffer[Size];
            std::unique_ptr<Zone> next;
        };

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
        template<class T, class... Args>
        T* allocate(size_t num_ops, Args&&... args) {
            static_assert(sizeof(Def) == sizeof(T),
                          "you are not allowed to introduce any additional data in subclasses of Def");
            Lock lock;
            size_t num_bytes = num_bytes_of<T>(num_ops);
            num_bytes        = align(num_bytes);
            assert(num_bytes < Zone::Size);

            if (index_ + num_bytes >= Zone::Size) {
                auto zone = new Zone;
                curr_->next.reset(zone);
                curr_  = zone;
                index_ = 0;
            }

            auto result = new (curr_->buffer + index_) T(std::forward<Args&&>(args)...);
            assert(result->num_ops() == num_ops);
            index_ += num_bytes;
            assert(index_ % alignof(T) == 0);

            return result;
        }

        template<class T>
        void deallocate(const T* def) {
            size_t num_bytes = num_bytes_of<T>(def->num_ops());
            num_bytes        = align(num_bytes);
            def->~T();
            if (ptrdiff_t(index_ - num_bytes) > 0) // don't care otherwise
                index_ -= num_bytes;
            assert(index_ % alignof(T) == 0);
        }

        static constexpr inline size_t align(size_t n) { return (n + (sizeof(void*) - 1)) & ~(sizeof(void*) - 1); }

        template<class T>
        static constexpr inline size_t num_bytes_of(size_t num_ops) {
            size_t result = sizeof(Def) + sizeof(void*) * num_ops;
            return align(result);
        }

        friend void swap(Arena& a1, Arena& a2) {
            using std::swap;
            // clang-format off
            swap(a1.root_,  a2.root_);
            swap(a1.curr_,  a2.curr_);
            swap(a1.index_, a2.index_);
            // clang-format on
        }

    private:
        std::unique_ptr<Zone> root_;
        Zone* curr_;
        size_t index_ = 0;
    } arena_;

    struct SeaHash {
        size_t operator()(const Def* def) const { return def->hash(); };
    };

    struct SeaEq {
        bool operator()(const Def* d1, const Def* d2) const { return d1->equal(d2); }
    };

    struct Move {
        Move(World&);

        std::unique_ptr<Checker> checker;
        absl::btree_map<u64, const Axiom*> axioms;
        absl::btree_map<Sym, Def*> externals;
        absl::flat_hash_set<const Def*, SeaHash, SeaEq> defs;
        DefDefMap<DefArray> cache;

        friend void swap(Move& m1, Move& m2) {
            using std::swap;
            // clang-format off
            swap(m1.checker,   m2.checker);
            swap(m1.axioms,    m2.axioms);
            swap(m1.externals, m2.externals);
            swap(m1.defs,      m2.defs);
            swap(m1.cache,     m2.cache);
            // clang-format on
            Checker::swap(*m1.checker, *m2.checker);
        }
    } move_;

    struct {
        const Univ* univ;
        const Type* type_0;
        const Type* type_1;
        const Bot* type_bot;
        const Def* type_bool;
        const Top* top_nat;
        const Sigma* sigma;
        const Tuple* tuple;
        const Nat* type_nat;
        const Idx* type_idx;
        const Def* table_id;
        const Def* table_not;
        std::array<const Lit*, 2> lit_bool;
        const Lit* lit_nat_0;
        const Lit* lit_nat_1;
        const Lit* lit_nat_max;
        const Lit* lit_univ_0;
        const Lit* lit_univ_1;
        Lam* exit;
    } data_;

    friend void swap(World& w1, World& w2) {
        using std::swap;
        // clang-format off
        swap(w1.state_, w2.state_);
        swap(w1.arena_, w2.arena_);
        swap(w1.data_,  w2.data_ );
        swap(w1.move_,  w2.move_ );
        // clang-format on

        swap(w1.data_.univ->world_, w2.data_.univ->world_);
        assert(&w1.univ()->world() == &w1);
        assert(&w2.univ()->world() == &w2);
    }

    friend DefArray Def::reduce(const Def*);
};

} // namespace thorin
