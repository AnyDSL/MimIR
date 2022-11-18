#pragma once

#include <sstream>
#include <string>
#include <string_view>

#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>

#include "thorin/axiom.h"
#include "thorin/check.h"
#include "thorin/config.h"
#include "thorin/debug.h"
#include "thorin/error.h"
#include "thorin/flags.h"
#include "thorin/lattice.h"
#include "thorin/tuple.h"

#include "thorin/util/hash.h"
#include "thorin/util/log.h"

namespace thorin {

class Checker;
class ErrorHandler;
class Scope;

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
        State(std::string_view name)
            : name(name) {}

        /// [Plain Old Data](https://en.cppreference.com/w/cpp/named_req/PODType)
        struct POD {
            Log log;
            Flags flags;
            u32 curr_gid        = 0;
            u32 curr_sub        = 0;
            mutable bool frozen = false;
        } pod;

        std::string name = "module";
        absl::btree_set<std::string> imported_dialects;
#if THORIN_ENABLE_CHECKS
        absl::flat_hash_set<u32> breakpoints;
#endif
        friend void swap(State& s1, State& s2) {
            using std::swap;
            // clang-format off
            swap(s1.pod,                s2.pod);
            swap(s1.name,               s2.name);
            swap(s1.imported_dialects,  s2.imported_dialects);
#if THORIN_ENABLE_CHECKS
            swap(s1.breakpoints,        s2.breakpoints);
#endif
            // clang-format on
        }
    };

    /// @name c'tor and d'tor
    ///@{
    World& operator=(const World&) = delete;

    /// Inherits the @p state into the new World.
    explicit World(const State&);
    explicit World(std::string_view name = {});
    World(World&& other)
        : World() {
        swap(*this, other);
    }
    ~World();
    ///@}

    /// @name misc getters/setters
    ///@{
    const State& state() const { return state_; }

    std::string_view name() const { return state_.name; }
    void set_name(std::string_view name) { state_.name = name; }

    void add_imported(std::string_view name) { state_.imported_dialects.emplace(name); }
    const auto& imported() const { return state_.imported_dialects; }

    /// Manage global identifier - a unique number for each Def.
    u32 curr_gid() const { return state_.pod.curr_gid; }
    u32 next_gid() { return ++state_.pod.curr_gid; }

    /// Retrive compile Flags.
    const Flags& flags() const { return state_.pod.flags; }
    Flags& flags() { return state_.pod.flags; }

    Checker& checker() {
        assert(&move_.checker->world() == this);
        return *move_.checker;
    }
    ErrorHandler* err() { return move_.err.get(); }
    ///@}

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
    void breakpoint(size_t number);
    const Def* gid2def(u32 gid);
    ///@}
#endif

    /// @name manage nodes
    ///@{
    const auto& axioms() const { return move_.axioms; }
    const auto& externals() const { return move_.externals; }
    bool empty() { return move_.externals.empty(); }
    void make_external(Def* def) {
        assert(!def->name().empty());
        auto name = def->name();
        move_.externals.emplace(def->name(), def); // TODO enable assert again
        // auto [i, ins] = move_.externals.emplace(def->name(), def);
        // assert((ins || (def == i->second)) && "two different externals registered with the same name");
    }
    void make_internal(Def* def) { move_.externals.erase(def->name()); }
    bool is_external(const Def* def) { return move_.externals.contains(def->name()); }
    Def* lookup(const std::string& name) {
        auto i = move_.externals.find(name);
        return i != move_.externals.end() ? i->second : nullptr;
    }
    ///@}

    /// @name Univ, Type, Var, Proxy, Infer
    ///@{
    const Univ* univ() { return data_.univ_; }
    const Type* type(const Def* level, const Def* dbg = {});
    template<level_t level = 0>
    const Type* type(const Def* dbg = {}) {
        if constexpr (level == 0)
            return data_.type_0_;
        else if constexpr (level == 1)
            return data_.type_1_;
        else
            return type(lit_univ(level), dbg);
    }
    const Var* var(const Def* type, Def* nom, const Def* dbg = {}) { return unify<Var>(1, type, nom, dbg); }
    const Proxy* proxy(const Def* type, Defs ops, u32 index, u32 tag, const Def* dbg = {}) {
        return unify<Proxy>(ops.size(), type, ops, index, tag, dbg);
    }
    Infer* nom_infer(const Def* type, const Def* dbg = {}) { return insert<Infer>(1, type, dbg); }
    Infer* nom_infer(const Def* type, Sym sym) { return insert<Infer>(1, type, dbg(sym)); }
    Infer* nom_infer_univ(const Def* dbg = {}) { return nom_infer(univ(), dbg); }
    Infer* nom_infer_type(const Def* dbg = {}) { return nom_infer(type_infer_univ(dbg), dbg); }
    const Type* type_infer_univ(const Def* dbg = {}) { return type(nom_infer_univ(dbg), dbg); }
    ///@}

    /// @name Axiom
    ///@{
    const Axiom* axiom(Def::NormalizeFn n, const Def* type, dialect_t d, tag_t t, sub_t s, const Def* dbg = {}) {
        auto ax                          = unify<Axiom>(0, n, type, d, t, s, dbg);
        return move_.axioms[ax->flags()] = ax;
    }
    const Axiom* axiom(const Def* type, dialect_t d, tag_t t, sub_t s, const Def* dbg = {}) {
        return axiom(nullptr, type, d, t, s, dbg);
    }

    /// Builds a fresh Axiom with descending Axiom::sub.
    /// This is useful during testing to come up with some entitiy of a specific type.
    /// It uses the dialect Axiom::Global_Dialect and starts with `0` for Axiom::sub and counts up from there.
    /// The Axiom::tag is set to `0` and the Axiom::normalizer to `nullptr`.
    const Axiom* axiom(const Def* type, const Def* dbg = {}) {
        return axiom(nullptr, type, Axiom::Global_Dialect, 0, state_.pod.curr_sub++, dbg);
    }

    /// Get Axiom from a dialect.
    /// Use this to get an Axiom via Axiom::id.
    template<class Id>
    const Axiom* ax(Id id) const {
        u64 flags = static_cast<u64>(id);
        if (auto i = move_.axioms.find(flags); i != move_.axioms.end()) return i->second;
        thorin::err("Axiom with ID '{}' not found; demangled dialect name is '{}'", flags, Axiom::demangle(flags));
    }

    /// Get Axiom from a dialect.
    /// Can be used to get an Axiom without sub-tags.
    /// E.g. use `w.ax<mem::M>();` to get the `%mem.M` Axiom.
    template<axiom_without_subs id>
    const Axiom* ax() const {
        return ax(Axiom::Base<id>);
    }
    ///@}

    /// @name Pi
    ///@{
    const Pi* pi(const Def* dom, const Def* codom, const Def* dbg = {}) {
        return unify<Pi>(2, codom->unfold_type(), dom, codom, dbg);
    }
    const Pi* pi(Defs dom, const Def* codom, const Def* dbg = {}) { return pi(sigma(dom), codom, dbg); }
    Pi* nom_pi(const Def* type, const Def* dbg = {}) { return insert<Pi>(2, type, dbg); }
    ///@}

    /// @name Cn (Pi with codom Bot)
    ///@{
    const Pi* cn() { return cn(sigma()); }
    const Pi* cn(const Def* dom, const Def* dbg = {}) { return pi(dom, type_bot(), dbg); }
    const Pi* cn(Defs doms, const Def* dbg = {}) { return cn(sigma(doms), dbg); }
    ///@}

    /// @name Lam
    ///@{
    Lam* nom_lam(const Pi* cn, const Def* dbg = {}) { return insert<Lam>(2, cn, dbg); }
    const Lam* lam(const Pi* pi, const Def* filter, const Def* body, const Def* dbg) {
        return unify<Lam>(2, pi, filter, body, dbg);
    }
    const Lam* lam(const Pi* pi, const Def* body, const Def* dbg) { return lam(pi, lit_tt(), body, dbg); }
    Lam* exit() { return data_.exit_; } ///< Used as a dummy exit node within Scope.
    ///@}

    /// @name App
    ///@{
    const Def* app(const Def* callee, const Def* arg, const Def* dbg = {});
    const Def* app(const Def* callee, Defs args, const Def* dbg = {}) { return app(callee, tuple(args), dbg); }
    /// Same as World::app but does *not* apply NormalizeFn.
    const Def* raw_app(const Def* callee, const Def* arg, const Def* dbg = {});
    /// Same as World::app but does *not* apply NormalizeFn.
    const Def* raw_app(const Def* callee, Defs args, const Def* dbg = {}) { return raw_app(callee, tuple(args), dbg); }
    ///@}

    /// @name Sigma
    ///@{
    Sigma* nom_sigma(const Def* type, size_t size, const Def* dbg = {}) { return insert<Sigma>(size, type, size, dbg); }
    /// A *nom*inal Sigma of type @p level.
    template<level_t level = 0>
    Sigma* nom_sigma(size_t size, const Def* dbg = {}) {
        return nom_sigma(type<level>(), size, dbg);
    }
    const Def* sigma(Defs ops, const Def* dbg = {});
    const Sigma* sigma() { return data_.sigma_; } ///< The unit type within Type 0.
    ///@}

    /// @name Arr
    ///@{
    Arr* nom_arr(const Def* type, const Def* dbg = {}) { return insert<Arr>(2, type, dbg); }
    template<level_t level = 0>
    Arr* nom_arr(const Def* dbg = {}) {
        return nom_arr(type<level>(), dbg);
    }
    const Def* arr(const Def* shape, const Def* body, const Def* dbg = {});
    const Def* arr(Defs shape, const Def* body, const Def* dbg = {});
    const Def* arr(u64 n, const Def* body, const Def* dbg = {}) { return arr(lit_nat(n), body, dbg); }
    const Def* arr(Span<u64> shape, const Def* body, const Def* dbg = {}) {
        return arr(DefArray(shape.size(), [&](size_t i) { return lit_nat(shape[i], dbg); }), body, dbg);
    }
    const Def* arr_unsafe(const Def* body, const Def* dbg = {}) { return arr(top_nat(), body, dbg); }
    ///@}

    /// @name Tuple
    ///@{
    const Def* tuple(Defs ops, const Def* dbg = {});
    /// Ascribes @p type to this tuple - needed for dependently typed and nominal Sigma%s.
    const Def* tuple(const Def* type, Defs ops, const Def* dbg = {});
    const Def* tuple_str(std::string_view s, const Def* dbg = {});
    Sym sym(std::string_view s, Loc loc) { return {tuple_str(s, dbg(loc)), loc.def(*this)}; }
    const Tuple* tuple() { return data_.tuple_; } ///< the unit value of type `[]`
    ///@}

    /// @name Pack
    ///@{
    Pack* nom_pack(const Def* type, const Def* dbg = {}) { return insert<Pack>(1, type, dbg); }
    const Def* pack(const Def* arity, const Def* body, const Def* dbg = {});
    const Def* pack(Defs shape, const Def* body, const Def* dbg = {});
    const Def* pack(u64 n, const Def* body, const Def* dbg = {}) { return pack(lit_nat(n), body, dbg); }
    const Def* pack(Span<u64> shape, const Def* body, const Def* dbg = {}) {
        return pack(DefArray(shape.size(), [&](auto i) { return lit_nat(shape[i], dbg); }), body, dbg);
    }
    ///@}

    /// @name Extract
    /// @sa core::extract_unsafe
    ///@{
    const Def* extract(const Def* d, const Def* i, const Def* dbg = {});
    const Def* extract(const Def* d, u64 a, u64 i, const Def* dbg = {}) { return extract(d, lit_idx(a, i), dbg); }
    const Def* extract(const Def* d, u64 i, const Def* dbg = {}) { return extract(d, as_lit(d->arity()), i, dbg); }

    /// Builds `(f, t)cond`.
    /// **Note** that select expects @p t as first argument and @p f as second one.
    const Def* select(const Def* t, const Def* f, const Def* cond, const Def* dbg = {}) {
        return extract(tuple({f, t}), cond, dbg);
    }
    ///@}

    /// @name Insert
    /// @sa core::insert_unsafe
    ///@{
    const Def* insert(const Def* d, const Def* i, const Def* val, const Def* dbg = {});
    const Def* insert(const Def* d, u64 a, u64 i, const Def* val, const Def* dbg = {}) {
        return insert(d, lit_idx(a, i), val, dbg);
    }
    const Def* insert(const Def* d, u64 i, const Def* val, const Def* dbg = {}) {
        return insert(d, as_lit(d->arity()), i, val, dbg);
    }
    ///@}

    /// @name Lit
    ///@{
    const Lit* lit(const Def* type, u64 val, const Def* dbg = {});
    const Lit* lit_univ(u64 level, const Def* dbg = {}) { return lit(univ(), level, dbg); }
    const Lit* lit_univ_0() { return data_.lit_univ_0_; }
    const Lit* lit_univ_1() { return data_.lit_univ_1_; }
    const Lit* lit_nat(nat_t a, const Def* dbg = {}) { return lit(type_nat(), a, dbg); }
    const Lit* lit_nat_0() { return data_.lit_nat_0_; }
    const Lit* lit_nat_1() { return data_.lit_nat_1_; }
    const Lit* lit_nat_max() { return data_.lit_nat_max_; }
    /// Constructs a Lit of type Idx of size @p size.
    /// @note `size = 0` means `2^64`.
    const Lit* lit_idx(nat_t size, u64 val, const Def* dbg = {}) { return lit(type_idx(size), val, dbg); }

    template<class I>
    const Lit* lit_idx(I val, const Def* dbg = {}) {
        static_assert(std::is_integral<I>());
        return lit_idx(Idx::bitwidth2size(sizeof(I) * 8), val, dbg);
    }

    /// Constructs a Lit @p of type Idx of size $2^width$.
    /// `val = 64` will be automatically converted to size `0` - the encoding for $2^64$.
    const Lit* lit_int(nat_t width, u64 val, const Def* dbg = {}) {
        return lit_idx(Idx::bitwidth2size(width), val, dbg);
    }

    /// Constructs a Lit of type Idx of size @p mod.
    /// The value @p val will be adjusted modulo @p mod.
    /// @note `mod == 0` is the special case for $2^64$ and no modulo will be performed on @p val.
    const Lit* lit_idx_mod(nat_t mod, u64 val, const Def* dbg = {}) {
        return lit_idx(mod, mod == 0 ? val : (val % mod), dbg);
    }

    const Lit* lit_bool(bool val) { return data_.lit_bool_[size_t(val)]; }
    const Lit* lit_ff() { return data_.lit_bool_[0]; }
    const Lit* lit_tt() { return data_.lit_bool_[1]; }
    // clang-format off
    ///@}

    /// @name lattice
    ///@{
    template<bool up>
    const Def* ext(const Def* type, const Def* dbg = {});
    const Def* bot(const Def* type, const Def* dbg = {}) { return ext<false>(type, dbg); }
    const Def* top(const Def* type, const Def* dbg = {}) { return ext<true>(type, dbg); }
    const Def* type_bot() { return data_.type_bot_; }
    const Def* top_nat() { return data_.top_nat_; }
    template<bool up> TBound<up>* nom_bound(const Def* type, size_t size, const Def* dbg = {}) { return insert<TBound<up>>(size, type, size, dbg); }
    /// A *nom*inal Bound of Type @p l%evel.
    template<bool up, level_t l = 0> TBound<up>* nom_bound(size_t size, const Def* dbg = {}) { return nom_bound<up>(type<l>(), size, dbg); }
    template<bool up> const Def* bound(Defs ops, const Def* dbg = {});
    Join* nom_join(const Def* type, size_t size, const Def* dbg = {}) { return nom_bound<true>(type, size, dbg); }
    Meet* nom_meet(const Def* type, size_t size, const Def* dbg = {}) { return nom_bound<false>(type, size, dbg); }
    template<level_t l = 0> Join* nom_join(size_t size, const Def* dbg = {}) { return nom_join(type<l>(), size, dbg); }
    template<level_t l = 0> Meet* nom_meet(size_t size, const Def* dbg = {}) { return nom_meet(type<l>(), size, dbg); }
    const Def* join(Defs ops, const Def* dbg = {}) { return bound<true>(ops, dbg); }
    const Def* meet(Defs ops, const Def* dbg = {}) { return bound<false>(ops, dbg); }
    const Def* ac(const Def* type, Defs ops, const Def* dbg = {});
    /// Infers the type using a *structural* Meet.
    const Def* ac(Defs ops, const Def* dbg = {});
    const Def* vel(const Def* type, const Def* value, const Def* dbg = {});
    const Def* pick(const Def* type, const Def* value, const Def* dbg = {});
    const Def* test(const Def* value, const Def* probe, const Def* match, const Def* clash, const Def* dbg = {});
    const Def* singleton(const Def* inner_type, const Def* dbg = {});
    ///@}

    /// @name globals -- depdrecated; will be removed
    ///@{
    Global* global(const Def* type, bool is_mutable = true, const Def* dbg = {}) { return insert<Global>(1, type, is_mutable, dbg); }
    ///@}
    // clang-format on

    /// @name types
    ///@{
    const Nat* type_nat() { return data_.type_nat_; }
    const Idx* type_idx() { return data_.type_idx_; }
    /// @note `size = 0` means `2^64`.
    const Def* type_idx(const Def* size, const Def* dbg = {}) { return app(type_idx(), size, dbg); }
    /// @note `size = 0` means `2^64`.
    const Def* type_idx(nat_t size) { return type_idx(lit_nat(size)); }

    /// Constructs a type Idx of size $2^width$.
    /// `width = 64` will be automatically converted to size `0` - the encoding for $2^64$.
    const Def* type_int(nat_t width) { return type_idx(lit_nat(Idx::bitwidth2size(width))); }
    const Def* type_bool() { return data_.type_bool_; }
    ///@}

    /// @name helpers
    ///@{
    const Def* dbg(Debug d) { return d.def(*this); }
    const Def* dbg(Sym sym, Loc loc, const Def* meta = {}) {
        meta = meta ? meta : bot(type_bot());
        return tuple({sym.str(), loc.def(*this), meta});
    }
    const Def* dbg(Sym sym, const Def* meta = {}) {
        auto loc = sym.loc() ? sym.loc() : Loc().def(*this);
        meta     = meta ? meta : bot(type_bot());
        return tuple({sym.str(), loc, meta});
    }
    const Def* iinfer(const Def* def) { return Idx::size(def->type()); }
    ///@}

    /// @name dumping/logging
    ///@{
    const Log& log() const { return state_.pod.log; }
    Log& log() { return state_.pod.log; }
    void dump(std::ostream& os) const;  ///< Dump to @p os.
    void dump() const;                  ///< Dump to `std::cout`.
    void debug_dump() const;            ///< Dump in Debug build if World::log::level is Log::Level::Debug.
    void write(const char* file) const; ///< Write to a file named @p file; defaults to World::name.
    void write() const;                 ///< Same above but file name defaults to World::name.
    ///@}

private:
    /// @name put into sea of nodes
    ///@{
    template<class T, class... Args>
    const T* unify(size_t num_ops, Args&&... args) {
        auto def = arena_.allocate<T>(num_ops, std::forward<Args&&>(args)...);
        assert(!def->isa_nom());
#if THORIN_ENABLE_CHECKS
        if (flags().trace_gids) outln("{}: {}", def->node_name(), def->gid());
        if (flags().reeval_breakpoints && state_.breakpoints.contains(def->gid())) thorin::breakpoint();
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
        if (!flags().reeval_breakpoints && state_.breakpoints.contains(def->gid())) thorin::breakpoint();
#endif
        def->finalize();
        return def;
    }

    template<class T, class... Args>
    T* insert(size_t num_ops, Args&&... args) {
        auto def = arena_.allocate<T>(num_ops, std::forward<Args&&>(args)...);
#if THORIN_ENABLE_CHECKS
        if (flags().trace_gids) outln("{}: {}", def->node_name(), def->gid());
        if (state_.breakpoints.contains(def->gid())) thorin::breakpoint();
#endif
        auto [_, ins] = move_.defs.emplace(def);
        assert_unused(ins);
        return def;
    }
    ///@}

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
            size_t result = sizeof(Def) + sizeof(const Def*) * num_ops;
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

    struct {
        const Univ* univ_;
        const Type* type_0_;
        const Type* type_1_;
        const Bot* type_bot_;
        const Def* type_bool_;
        const Top* top_nat_;
        const Sigma* sigma_;
        const Tuple* tuple_;
        const Nat* type_nat_;
        const Idx* type_idx_;
        const Def* table_id;
        const Def* table_not;
        std::array<const Lit*, 2> lit_bool_;
        const Lit* lit_nat_0_;
        const Lit* lit_nat_1_;
        const Lit* lit_nat_max_;
        const Lit* lit_univ_0_;
        const Lit* lit_univ_1_;
        Lam* exit_;
    } data_;

    struct Move {
        Move(World&);

        absl::btree_map<u64, const Axiom*> axioms;
        absl::btree_map<std::string, Def*> externals;
        absl::flat_hash_set<const Def*, SeaHash, SeaEq> defs;
        DefDefMap<DefArray> cache;
        std::unique_ptr<Checker> checker;
        std::unique_ptr<ErrorHandler> err;

        friend void swap(Move& m1, Move& m2) {
            using std::swap;
            // clang-format off
            swap(m1.axioms,    m2.axioms);
            swap(m1.externals, m2.externals);
            swap(m1.defs,      m2.defs);
            swap(m1.cache,     m2.cache);
            swap(m1.checker,   m2.checker);
            swap(m1.err,       m2.err);
            // clang-format on
            Checker::swap(*m1.checker, *m2.checker);
        }
    } move_;

    friend void swap(World& w1, World& w2) {
        using std::swap;
        // clang-format off
        swap(w1.state_, w2.state_);
        swap(w1.arena_, w2.arena_);
        swap(w1.data_,  w2.data_ );
        swap(w1.move_,  w2.move_ );
        // clang-format on

        swap(w1.data_.univ_->world_, w2.data_.univ_->world_);
        assert(&w1.univ()->world() == &w1);
        assert(&w2.univ()->world() == &w2);
    }

    friend DefArray Def::reduce(const Def*);
};

} // namespace thorin
