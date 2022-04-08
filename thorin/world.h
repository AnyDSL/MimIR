#ifndef THORIN_WORLD_H
#define THORIN_WORLD_H

#include <cassert>

#include <functional>
#include <initializer_list>
#include <iostream>
#include <string>

#include "thorin/axiom.h"
#include "thorin/config.h"
#include "thorin/lattice.h"
#include "thorin/tuple.h"

#include "thorin/util/hash.h"

namespace thorin {

enum class LogLevel { Debug, Verbose, Info, Warn, Error };

class Checker;
class DepNode;
class ErrorHandler;
class RecStreamer;
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
class World : public Streamable<World> {
public:
    World& operator=(const World&) = delete;

    explicit World(std::string_view name = {});
    World(World&& other)
        : World() {
        swap(*this, other);
    }
    ~World();

    /// Inherits the World::state_ of the @p other World.
    World stub();

    /// @name Sea of Nodes
    ///@{
    struct SeaHash {
        size_t operator()(const Def* def) const { return def->hash(); };
    };

    struct SeaEq {
        bool operator()(const Def* d1, const Def* d2) const { return d1->equal(d2); }
    };

    using Sea = absl::flat_hash_set<const Def*, SeaHash, SeaEq>; ///< This HashSet contains Thorin's "sea of nodes".

    const Sea& defs() const { return data_.defs_; }
    ///@}

    /// @name name
    ///@{
    std::string_view name() const { return data_.name_; }
    void set_name(std::string_view name) { data_.name_ = name; }
    ///@}

    /// @name manage global identifier - a unique number for each Def
    ///@{
    u32 curr_gid() const { return state_.curr_gid; }
    u32 next_gid() { return ++state_.curr_gid; }
    ///@}

    /// @name Universe, Type, Var, Proxy, Infer
    ///@{
    const Univ* univ() { return data_.univ_; }
    const Type* type(const Def* level) { return unify<Type>(1, level)->as<Type>(); }
    template<level_t level = 0>
    const Type* type() {
        if constexpr (level == 0)
            return data_.type_0_;
        else if constexpr (level == 1)
            return data_.type_1_;
        else
            return type(lit_univ(level));
    }
    const Var* var(const Def* type, Def* nom, const Def* dbg = {}) { return unify<Var>(1, type, nom, dbg); }
    const Proxy* proxy(const Def* type, Defs ops, tag_t index, flags_t flags, const Def* dbg = {}) {
        return unify<Proxy>(ops.size(), type, ops, index, flags, dbg);
    }
    Infer* nom_infer(const Def* type, const Def* dbg = {}) { return insert<Infer>(1, type, dbg); }
    Infer* nom_infer(const Def* type, Sym sym, Loc loc) { return insert<Infer>(1, type, dbg({sym, loc})); }
    Infer* nom_infer_univ(const Def* dbg = {}) { return nom_infer(univ(), dbg); }
    ///@}

    /// @name Axiom
    ///@{
    const Axiom* axiom(Def::NormalizeFn normalize, const Def* type, tag_t tag, flags_t flags, const Def* dbg = {}) {
        return unify<Axiom>(0, normalize, type, tag, flags, dbg);
    }
    const Axiom* axiom(const Def* type, tag_t tag, flags_t flags, const Def* dbg = {}) {
        return axiom(nullptr, type, tag, flags, dbg);
    }

    /// Builds a fresh Axiom with descending tag.
    /// This is useful during testing to come up with some entitiy of a specific type.
    /// It starts with `tag_t(-1)` (aka max) for Axiom::tag and counts down from there.
    /// The Axiom::flags are set to `0` and the Axiom::normalizer to `nullptr`.
    const Axiom* axiom(const Def* type, const Def* dbg = {}) { return axiom(nullptr, type, state_.curr_tag--, 0, dbg); }
    ///@}

    /// @name Pi
    ///@{
    const Pi* pi(const Def* dom, const Def* codom, const Def* dbg = {}) {
        return unify<Pi>(2, codom->inf_type(), dom, codom, dbg);
    }
    const Pi* pi(Defs dom, const Def* codom, const Def* dbg = {}) { return pi(sigma(dom), codom, dbg); }
    Pi* nom_pi(const Def* type, const Def* dbg = {}) { return insert<Pi>(2, type, dbg); }
    ///@}

    /// @name Pi: continuation type (cn), i.e., Pi type with codom Bottom
    ///@{
    const Pi* cn() { return cn(sigma()); }
    const Pi* cn(const Def* dom, const Def* dbg = {}) { return pi(dom, bot_type(), dbg); }
    const Pi* cn(Defs doms, const Def* dbg = {}) { return cn(sigma(doms), dbg); }
    /// Same as World::cn / World::pi but adds a World::type_mem-typed Var to each Pi.
    const Pi* cn_mem(const Def* dom, const Def* dbg = {}) { return cn({type_mem(), dom}, dbg); }
    const Pi* cn_mem_ret(const Def* dom, const Def* ret_dom, const Def* dbg = {}) {
        return cn({type_mem(), dom, cn_mem(ret_dom)}, dbg);
    }
    const Pi* pi_mem(const Def* domain, const Def* codomain, const Def* dbg = {}) {
        auto d = sigma({type_mem(), domain});
        return pi(d, sigma({type_mem(), codomain}), dbg);
    }
    const Pi* fn_mem(const Def* domain, const Def* codomain, const Def* dbg = {}) {
        return cn({type_mem(), domain, cn_mem(codomain)}, dbg);
    }
    ///@}

    /// @name Lam%bda
    ///@{
    Lam* nom_lam(const Pi* cn, Lam::CC cc = Lam::CC::C, const Def* dbg = {}) {
        auto lam = insert<Lam>(2, cn, cc, dbg);
        return lam;
    }
    Lam* nom_lam(const Pi* cn, const Def* dbg = {}) { return nom_lam(cn, Lam::CC::C, dbg); }
    const Lam* lam(const Pi* pi, const Def* filter, const Def* body, const Def* dbg) {
        return unify<Lam>(2, pi, filter, body, dbg);
    }
    const Lam* lam(const Pi* pi, const Def* body, const Def* dbg) { return lam(pi, lit_true(), body, dbg); }
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
    const Def* arr(ArrayRef<u64> shape, const Def* body, const Def* dbg = {}) {
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
    Sym sym(std::string_view s, const Def* dbg = {}) { return tuple_str(s, dbg); }
    const Tuple* tuple() { return data_.tuple_; } ///< the unit value of type `[]`
    ///@}

    /// @name Pack
    ///@{
    Pack* nom_pack(const Def* type, const Def* dbg = {}) { return insert<Pack>(1, type, dbg); }
    const Def* pack(const Def* arity, const Def* body, const Def* dbg = {});
    const Def* pack(Defs shape, const Def* body, const Def* dbg = {});
    const Def* pack(u64 n, const Def* body, const Def* dbg = {}) { return pack(lit_nat(n), body, dbg); }
    const Def* pack(ArrayRef<u64> shape, const Def* body, const Def* dbg = {}) {
        return pack(DefArray(shape.size(), [&](auto i) { return lit_nat(shape[i], dbg); }), body, dbg);
    }
    ///@}

    /// @name Extract
    ///@{
    const Def* extract(const Def* tup, const Def* i, const Def* dbg = {}) { return extract_(nullptr, tup, i, dbg); }
    const Def* extract(const Def* tup, u64 a, u64 i, const Def* dbg = {}) {
        return extract_(nullptr, tup, lit_int(a, i), dbg);
    }
    const Def* extract(const Def* tup, u64 i, const Def* dbg = {}) {
        return extract(tup, as_lit(tup->arity()), i, dbg);
    }
    const Def* extract_unsafe(const Def* tup, u64 i, const Def* dbg = {}) {
        return extract_unsafe(tup, lit_int(0_u64, i), dbg);
    }
    const Def* extract_unsafe(const Def* tup, const Def* i, const Def* dbg = {}) {
        return extract(tup, op(Conv::u2u, type_int(as_lit(tup->type()->reduce_rec()->arity())), i, dbg), dbg);
    }
    /// During a rebuild we cannot infer the type if it is not set yet; in this case we rely on @p ex_type.
    const Def* extract_(const Def* ex_type, const Def* tup, const Def* i, const Def* dbg = {});
    /// Builds `(f, t)cond`.
    /// **Note** that select expects @p t as first argument and @p f as second one.
    const Def* select(const Def* t, const Def* f, const Def* cond, const Def* dbg = {}) {
        return extract(tuple({f, t}), cond, dbg);
    }
    ///@}

    /// @name Insert
    ///@{
    const Def* insert(const Def* tup, const Def* i, const Def* val, const Def* dbg = {});
    const Def* insert(const Def* tup, u64 a, u64 i, const Def* val, const Def* dbg = {}) {
        return insert(tup, lit_int(a, i), val, dbg);
    }
    const Def* insert(const Def* tup, u64 i, const Def* val, const Def* dbg = {}) {
        return insert(tup, as_lit(tup->arity()), i, val, dbg);
    }
    const Def* insert_unsafe(const Def* tup, u64 i, const Def* val, const Def* dbg = {}) {
        return insert_unsafe(tup, lit_int(0_u64, i), val, dbg);
    }
    const Def* insert_unsafe(const Def* tup, const Def* i, const Def* val, const Def* dbg = {}) {
        return insert(tup, op(Conv::u2u, type_int(as_lit(tup->type()->reduce_rec()->arity())), i), val, dbg);
    }
    ///@}

    /// @name Lit
    ///@{
    const Lit* lit(const Def* type, u64 val, const Def* dbg = {}) { return unify<Lit>(0, type, val, dbg); }
    const Lit* lit_nat(nat_t a, const Def* dbg = {}) { return lit(type_nat(), a, dbg); }
    const Lit* lit_nat_0() { return data_.lit_nat_0_; }
    const Lit* lit_nat_1() { return data_.lit_nat_1_; }
    const Lit* lit_nat_max() { return data_.lit_nat_max_; }
    const Lit* lit_int(const Def* type, u64 val, const Def* dbg = {});
    const Lit* lit_univ(u64 level, const Def* dbg = {}) { return lit(univ(), level, dbg); }
    const Lit* lit_univ_0() { return data_.lit_univ_0_; }
    const Lit* lit_univ_1() { return data_.lit_univ_1_; }

    /// Constructs Tag::Int Lit @p val via @p width, i.e. converts from *width* to *internal* *mod* value.
    const Lit* lit_int_width(nat_t width, u64 val, const Def* dbg = {}) {
        return lit_int(type_int_width(width), val, dbg);
    }

    /// Constructs Tag::Int Lit @p val with *external* *mod*.
    /// I.e. if `mod == 0`, it will be adjusted to `uint_t(-1)` (special case for `2^64`).
    const Lit* lit_int_mod(nat_t mod, u64 val, const Def* dbg = {}) {
        return lit_int(type_int(mod), mod == 0 ? val : (val % mod), dbg);
    }

    /// Constructs Tag::Int Lit @p val with *internal* *mod*, i.e. without any conversions - `mod = 0` means `2^64`.
    /// Use this version if you directly receive an *internal* `mod` which is already converted.
    const Lit* lit_int(nat_t mod, u64 val, const Def* dbg = {}) { return lit_int(type_int(mod), val, dbg); }
    template<class I>
    const Lit* lit_int(I val, const Def* dbg = {}) {
        static_assert(std::is_integral<I>());
        return lit_int(type_int(width2mod(sizeof(I) * 8)), val, dbg);
    }

    const Lit* lit_bool(bool val) { return data_.lit_bool_[size_t(val)]; }
    const Lit* lit_false() { return data_.lit_bool_[0]; }
    const Lit* lit_true() { return data_.lit_bool_[1]; }
    // clang-format off
    const Lit* lit_real(nat_t width, r64 val, const Def* dbg = {}) {
        switch (width) {
            case 16: assert(r64(r16(r32(val))) == val && "loosing precision"); return lit_real(r16(r32(val)), dbg);
            case 32: assert(r64(r32(   (val))) == val && "loosing precision"); return lit_real(r32(   (val)), dbg);
            case 64: assert(r64(r64(   (val))) == val && "loosing precision"); return lit_real(r64(   (val)), dbg);
            default: unreachable();
        }
    }
    template<class R>
    const Lit* lit_real(R val, const Def* dbg = {}) {
        static_assert(std::is_floating_point<R>() || std::is_same<R, r16>());
        if constexpr (false) {}
        else if constexpr (sizeof(R) == 2) return lit(type_real(16), thorin::bitcast<u16>(val), dbg);
        else if constexpr (sizeof(R) == 4) return lit(type_real(32), thorin::bitcast<u32>(val), dbg);
        else if constexpr (sizeof(R) == 8) return lit(type_real(64), thorin::bitcast<u64>(val), dbg);
        else unreachable();
    }
    ///@}

    /// @name lattice
    ///@{
    template<bool up>
    const Def* ext(const Def* type, const Def* dbg = {});
    const Def* bot(const Def* type, const Def* dbg = {}) { return ext<false>(type, dbg); }
    const Def* top(const Def* type, const Def* dbg = {}) { return ext<true>(type, dbg); }
    const Def* bot_type() { return data_.bot_type_; }
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
    const Def* et(const Def* type, Defs ops, const Def* dbg = {});
    /// Infers the type using a *structural* Meet.
    const Def* et(Defs ops, const Def* dbg = {}) { return et(infer_type(ops), ops, dbg); }
    const Def* vel(const Def* type, const Def* value, const Def* dbg = {});
    const Def* pick(const Def* type, const Def* value, const Def* dbg = {});
    const Def* test(const Def* value, const Def* probe, const Def* match, const Def* clash, const Def* dbg = {});
    ///@}

    /// @name globals -- depdrecated; will be removed
    ///@{
    Global* global(const Def* type, bool is_mutable = true, const Def* dbg = {}) { return insert<Global>(1, type, is_mutable, dbg); }
    Global* global_immutable_string(std::string_view str, const Def* dbg = {});
    ///@}
    // clang-format on

    /// @name types
    ///@{
    const Nat* type_nat() { return data_.type_nat_; }
    const Axiom* type_mem() { return data_.type_mem_; }
    const Axiom* type_int() { return data_.type_int_; }
    const Axiom* type_real() { return data_.type_real_; }
    const Axiom* type_ptr() { return data_.type_ptr_; }
    const App* type_bool() { return data_.type_bool_; }
    const App* type_int_width(nat_t width) { return type_int(lit_nat(width2mod(width))); }
    const App* type_int(nat_t mod) { return type_int(lit_nat(mod)); }
    const App* type_real(nat_t width) { return type_real(lit_nat(width)); }
    const App* type_int(const Def* mod) { return app(type_int(), mod)->as<App>(); }
    const App* type_real(const Def* width) { return app(type_real(), width)->as<App>(); }
    const App* type_ptr(const Def* pointee, nat_t addr_space = AddrSpace::Generic, const Def* dbg = {}) {
        return type_ptr(pointee, lit_nat(addr_space), dbg);
    }
    const App* type_ptr(const Def* pointee, const Def* addr_space, const Def* dbg = {}) {
        return app(type_ptr(), {pointee, addr_space}, dbg)->as<App>();
    }
    ///@}

    /// @name bulitin axioms
    ///@{
    // clang-format off
    const Axiom* ax(Acc   o)  const { return data_.Acc_  [size_t(o)]; }
    const Axiom* ax(Bit   o)  const { return data_.Bit_  [size_t(o)]; }
    const Axiom* ax(Conv  o)  const { return data_.Conv_ [size_t(o)]; }
    const Axiom* ax(Div   o)  const { return data_.Div_  [size_t(o)]; }
    const Axiom* ax(ICmp  o)  const { return data_.ICmp_ [size_t(o)]; }
    const Axiom* ax(PE    o)  const { return data_.PE_   [size_t(o)]; }
    const Axiom* ax(RCmp  o)  const { return data_.RCmp_ [size_t(o)]; }
    const Axiom* ax(ROp   o)  const { return data_.ROp_  [size_t(o)]; }
    const Axiom* ax(Shr   o)  const { return data_.Shr_  [size_t(o)]; }
    const Axiom* ax(Trait o)  const { return data_.Trait_[size_t(o)]; }
    const Axiom* ax(Wrap  o)  const { return data_.Wrap_ [size_t(o)]; }
    const Axiom* ax_alloc()   const { return data_.alloc_;   }
    const Axiom* ax_atomic()  const { return data_.atomic_;  }
    const Axiom* ax_bitcast() const { return data_.bitcast_; }
    const Axiom* ax_lea()     const { return data_.lea_;     }
    const Axiom* ax_malloc()  const { return data_.malloc_;  }
    const Axiom* ax_mslot()   const { return data_.mslot_;   }
    const Axiom* ax_zip()     const { return data_.zip_;     }
    const Axiom* ax_for()     const { return data_.for_;     }
    const Axiom* ax_load()    const { return data_.load_;    }
    const Axiom* ax_remem()   const { return data_.remem_;   }
    const Axiom* ax_slot()    const { return data_.slot_;    }
    const Axiom* ax_store()   const { return data_.store_;   }
    // clang-format on
    ///@}

    /// @name fn - these guys yield the final function to be invoked for the various operations
    ///@{
    const Def* fn(Bit o, const Def* mod, const Def* dbg = {}) { return app(ax(o), mod, dbg); }
    const Def* fn(Conv o, const Def* dst_w, const Def* src_w, const Def* dbg = {}) {
        return app(ax(o), {dst_w, src_w}, dbg);
    }
    const Def* fn(Div o, const Def* mod, const Def* dbg = {}) { return app(ax(o), mod, dbg); }
    const Def* fn(ICmp o, const Def* mod, const Def* dbg = {}) { return app(ax(o), mod, dbg); }
    const Def* fn(RCmp o, const Def* rmode, const Def* width, const Def* dbg = {}) {
        return app(ax(o), {rmode, width}, dbg);
    }
    const Def* fn(ROp o, const Def* rmode, const Def* width, const Def* dbg = {}) {
        return app(ax(o), {rmode, width}, dbg);
    }
    const Def* fn(Shr o, const Def* mod, const Def* dbg = {}) { return app(ax(o), mod, dbg); }
    const Def* fn(Wrap o, const Def* wmode, const Def* mod, const Def* dbg = {}) {
        return app(ax(o), {wmode, mod}, dbg);
    }
    template<class O>
    const Def* fn(O o, nat_t size, const Def* dbg = {}) {
        return fn(o, lit_nat(size), dbg);
    }
    template<class O>
    const Def* fn(O o, nat_t other, nat_t size, const Def* dbg = {}) {
        return fn(o, lit_nat(other), lit_nat(size), dbg);
    }
    const Def* fn_atomic(const Def* fn, const Def* dbg = {}) { return app(ax_atomic(), fn, dbg); }
    const Def* fn_bitcast(const Def* dst_t, const Def* src_t, const Def* dbg = {}) {
        return app(ax_bitcast(), {dst_t, src_t}, dbg);
    }
    const Def* fn_for(Defs params);
    ///@}

    /// @name op - these guys build the final function application for the various operations
    ///@{
    const Def* op(Bit o, const Def* a, const Def* b, const Def* dbg = {}) { return app(fn(o, infer(a)), {a, b}, dbg); }
    const Def* op(Div o, const Def* mem, const Def* a, const Def* b, const Def* dbg = {}) {
        return app(fn(o, infer(a)), {mem, a, b}, dbg);
    }
    const Def* op(ICmp o, const Def* a, const Def* b, const Def* dbg = {}) { return app(fn(o, infer(a)), {a, b}, dbg); }
    const Def* op(RCmp o, const Def* rmode, const Def* a, const Def* b, const Def* dbg = {}) {
        return app(fn(o, rmode, infer(a)), {a, b}, dbg);
    }
    const Def* op(ROp o, const Def* rmode, const Def* a, const Def* b, const Def* dbg = {}) {
        return app(fn(o, rmode, infer(a)), {a, b}, dbg);
    }
    const Def* op(Shr o, const Def* a, const Def* b, const Def* dbg = {}) { return app(fn(o, infer(a)), {a, b}, dbg); }
    const Def* op(Wrap o, const Def* wmode, const Def* a, const Def* b, const Def* dbg = {}) {
        return app(fn(o, wmode, infer(a)), {a, b}, dbg);
    }
    template<class O>
    const Def* op(O o, nat_t mode, const Def* a, const Def* b, const Def* dbg = {}) {
        return op(o, lit_nat(mode), a, b, dbg);
    }
    const Def* op(Conv o, const Def* dst_type, const Def* src, const Def* dbg = {}) {
        auto d = dst_type->as<App>()->arg();
        auto s = src->type()->as<App>()->arg();
        return app(fn(o, d, s), src, dbg);
    }
    const Def* op(Trait o, const Def* type, const Def* dbg = {}) { return app(ax(o), type, dbg); }
    const Def* op(PE o, const Def* def, const Def* dbg = {}) { return app(app(ax(o), def->type()), def, dbg); }
    const Def* op(Acc o, const Def* a, const Def* b, const Def* body, const Def* dbg = {}) {
        return app(ax(o), {a, b, body}, dbg);
    }
    const Def* op_atomic(const Def* fn, Defs args, const Def* dbg = {}) { return app(fn_atomic(fn), args, dbg); }
    const Def* op_bitcast(const Def* dst_type, const Def* src, const Def* dbg = {}) {
        return app(fn_bitcast(dst_type, src->type()), src, dbg);
    }
    const Def* op_lea(const Def* ptr, const Def* index, const Def* dbg = {});
    const Def* op_lea_unsafe(const Def* ptr, u64 i, const Def* dbg = {}) { return op_lea_unsafe(ptr, lit_int(i), dbg); }
    const Def* op_lea_unsafe(const Def* ptr, const Def* i, const Def* dbg = {}) {
        auto safe_int = type_int(as<Tag::Ptr>(ptr->type())->arg(0)->arity());
        return op_lea(ptr, op(Conv::u2u, safe_int, i), dbg);
    }
    const Def* op_remem(const Def* mem, const Def* dbg = {}) { return app(ax_remem(), mem, dbg); }
    const Def* op_load(const Def* mem, const Def* ptr, const Def* dbg = {}) {
        auto [T, a] = as<Tag::Ptr>(ptr->type())->args<2>();
        return app(app(ax_load(), {T, a}), {mem, ptr}, dbg);
    }
    const Def* op_store(const Def* mem, const Def* ptr, const Def* val, const Def* dbg = {}) {
        auto [T, a] = as<Tag::Ptr>(ptr->type())->args<2>();
        return app(app(ax_store(), {T, a}), {mem, ptr, val}, dbg);
    }
    const Def* op_alloc(const Def* type, const Def* mem, const Def* dbg = {}) {
        return app(app(ax_alloc(), {type, lit_nat_0()}), mem, dbg);
    }
    const Def* op_slot(const Def* type, const Def* mem, const Def* dbg = {}) {
        return app(app(ax_slot(), {type, lit_nat_0()}), {mem, lit_nat(curr_gid())}, dbg);
    }
    const Def* op_malloc(const Def* type, const Def* mem, const Def* dbg = {});
    const Def* op_mslot(const Def* type, const Def* mem, const Def* id, const Def* dbg = {});
    // clang-format off
    const Def* op_for(const Def* mem, const Def* start, const Def* stop, const Def* step, Defs inits, const Def* body, const Def* brk);
    // clang-format on
    ///@}

    /// @name wrappers for unary operations
    ///@{
    const Def* op_negate(const Def* a, const Def* dbg = {}) {
        auto w = as_lit(infer(a));
        return op(Bit::_xor, lit_int(w, w - 1_u64), a, dbg);
    }
    const Def* op_rminus(const Def* rmode, const Def* a, const Def* dbg = {}) {
        auto w = as_lit(infer(a));
        return op(ROp::sub, rmode, lit_real(w, -0.0), a, dbg);
    }
    const Def* op_wminus(const Def* wmode, const Def* a, const Def* dbg = {}) {
        auto w = as_lit(infer(a));
        return op(Wrap::sub, wmode, lit_int(w, 0), a, dbg);
    }
    const Def* op_rminus(nat_t rmode, const Def* a, const Def* dbg = {}) { return op_rminus(lit_nat(rmode), a, dbg); }
    const Def* op_wminus(nat_t wmode, const Def* a, const Def* dbg = {}) { return op_wminus(lit_nat(wmode), a, dbg); }
    ///@}

    /// @name helpers
    ///@{
    const Def* dbg(Debug);
    const Def* infer(const Def* def) { return isa_sized_type(def->type()); }
    const Def* infer_type(Defs);
    ///@}

    /// @name partial evaluation done?
    ///@{
    void mark_pe_done(bool flag = true) { state_.pe_done = flag; }
    bool is_pe_done() const { return state_.pe_done; }
    ///@}

    /// @name Manage Externals
    ///@{
    using Externals = absl::flat_hash_map<std::string, Def*>;
    const Externals& externals() const { return data_.externals_; }
    bool empty() { return data_.externals_.empty(); }
    void make_external(Def* def) { data_.externals_.emplace(def->name(), def); }
    void make_internal(Def* def) { data_.externals_.erase(def->name()); }
    bool is_external(const Def* def) { return data_.externals_.contains(def->name()); }
    Def* lookup(std::string_view name) {
        auto i = data_.externals_.find(name);
        return i != data_.externals_.end() ? i->second : nullptr;
    }

    using VisitFn = std::function<void(const Scope&)>;
    /// Transitively visits all *reachable* Scope%s in this World that do not have free variables.
    /// We call these Scope%s *top-level* Scope%s.
    /// Select with @p elide_empty whether you want to visit trivial Scope%s of *noms* without body.
    template<bool elide_empty = true>
    void visit(VisitFn) const;
    ///@}

#if THORIN_ENABLE_CHECKS
    /// @name Debugging Features
    ///@{
    using Breakpoints = absl::flat_hash_set<u32>;

    void breakpoint(size_t number);
    void use_breakpoint(size_t number);
    void enable_history(bool flag = true);
    bool track_history() const;
    const Def* gid2def(u32 gid);
    ///@}
#endif

    /// @name Logging
    ///@{
    Stream& stream() { return *stream_; }
    LogLevel min_level() const { return state_.min_level; }

    void set_log_level(LogLevel min_level) { state_.min_level = min_level; }
    void set_log_level(std::string_view min_level) { set_log_level(str2level(min_level)); }
    void set_log_stream(std::shared_ptr<Stream> stream) { stream_ = stream; }

    template<class... Args>
    void log(LogLevel level, Loc loc, const char* fmt, Args&&... args) {
        if (stream_ && int(min_level()) <= int(level)) {
            stream().fmt("{}:{}: ", colorize(level2acro(level), level2color(level)), colorize(loc.to_string(), 7));
            stream().fmt(fmt, std::forward<Args&&>(args)...).endl().flush();
        }
    }
    void log() const {} ///< for DLOG in Release build.

    template<class... Args>
    [[noreturn]] void error(Loc loc, const char* fmt, Args&&... args) {
        log(LogLevel::Error, loc, fmt, std::forward<Args&&>(args)...);
        std::abort();
    }

    // clang-format off
    template<class... Args> void idef(const Def* def, const char* fmt, Args&&... args) { log(LogLevel::Info, def->loc(), fmt, std::forward<Args&&>(args)...); }
    template<class... Args> void wdef(const Def* def, const char* fmt, Args&&... args) { log(LogLevel::Warn, def->loc(), fmt, std::forward<Args&&>(args)...); }
    template<class... Args> void edef(const Def* def, const char* fmt, Args&&... args) { error(def->loc(), fmt, std::forward<Args&&>(args)...); }
    // clang-format on

    static std::string_view level2acro(LogLevel);
    static LogLevel str2level(std::string_view);
    static int level2color(LogLevel level);
    static std::string colorize(std::string_view str, int color);
    ///@}

    /// @name stream
    ///@{
    Stream& stream(Stream&) const;
    Stream& stream(RecStreamer&, const DepNode*) const;
    void debug_stream(); ///< Stream thorin if World::State::min_level is LogLevel::debug.
    ///@}

    /// @name error handling
    ///@{
    void set_error_handler(std::unique_ptr<ErrorHandler>&& err);
    ErrorHandler* err() { return err_.get(); }
    ///@}

    friend void swap(World& w1, World& w2) {
        using std::swap;
        // clang-format off
        swap(w1.arena_,   w2.arena_);
        swap(w1.data_,    w2.data_);
        swap(w1.state_,   w2.state_);
        swap(w1.stream_,  w2.stream_);
        swap(w1.checker_, w2.checker_);
        swap(w1.err_,     w2.err_);
        // clang-format on

        swap(w1.data_.univ_->world_, w2.data_.univ_->world_);
        assert(&w1.univ()->world() == &w1);
        assert(&w2.univ()->world() == &w2);
    }

private:
    /// @name put into sea of nodes
    ///@{
    template<class T, class... Args>
    const T* unify(size_t num_ops, Args&&... args) {
        auto def = arena_.allocate<T>(num_ops, std::forward<Args&&>(args)...);
        assert(!def->isa_nom());
        auto [i, inserted] = data_.defs_.emplace(def);
        if (inserted) {
#if THORIN_ENABLE_CHECKS
            if (state_.breakpoints.contains(def->gid())) thorin::breakpoint();
            for (auto op : def->ops()) {
                if (state_.use_breakpoints.contains(op->gid())) thorin::breakpoint();
            }
#endif
            def->finalize();
            return def;
        }

        arena_.deallocate<T>(def);
        return static_cast<const T*>(*i);
    }

    template<class T, class... Args>
    T* insert(size_t num_ops, Args&&... args) {
        auto def = arena_.allocate<T>(num_ops, std::forward<Args&&>(args)...);
#if THORIN_ENABLE_CHECKS
        if (state_.breakpoints.contains(def->gid())) thorin::breakpoint();
#endif
        auto p = data_.defs_.emplace(def);
        assert_unused(p.second);
        return def;
    }
    ///@}

    class Arena {
    public:
        Arena()
            : root_zone_(new Zone) // don't use 'new Zone()' - we keep the allocated Zone uninitialized
            , curr_zone_(root_zone_.get()) {}

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

            if (buffer_index_ + num_bytes >= Zone::Size) {
                auto zone = new Zone;
                curr_zone_->next.reset(zone);
                curr_zone_    = zone;
                buffer_index_ = 0;
            }

            auto result = new (curr_zone_->buffer + buffer_index_) T(std::forward<Args&&>(args)...);
            assert(result->num_ops() == num_ops);
            buffer_index_ += num_bytes;
            assert(buffer_index_ % alignof(T) == 0);

            return result;
        }

        template<class T>
        void deallocate(const T* def) {
            size_t num_bytes = num_bytes_of<T>(def->num_ops());
            num_bytes        = align(num_bytes);
            def->~T();
            if (ptrdiff_t(buffer_index_ - num_bytes) > 0) // don't care otherwise
                buffer_index_ -= num_bytes;
            assert(buffer_index_ % alignof(T) == 0);
        }

        static constexpr inline size_t align(size_t n) { return (n + (sizeof(void*) - 1)) & ~(sizeof(void*) - 1); }

        template<class T>
        static constexpr inline size_t num_bytes_of(size_t num_ops) {
            size_t result = sizeof(Def) + sizeof(const Def*) * num_ops;
            return align(result);
        }

    private:
        std::unique_ptr<Zone> root_zone_;
        Zone* curr_zone_;
        size_t buffer_index_ = 0;
    } arena_;

    struct State {
        LogLevel min_level = LogLevel::Error;
        u32 curr_gid       = 0;
        u32 curr_tag       = tag_t(-1);
        bool pe_done       = false;
#if THORIN_ENABLE_CHECKS
        bool track_history = false;
        Breakpoints breakpoints;
        Breakpoints use_breakpoints;
#endif
    } state_;

    struct Data {
        const Univ* univ_;
        const Type* type_0_;
        const Type* type_1_;
        const Bot* bot_type_;
        const App* type_bool_;
        const Top* top_nat_;
        const Sigma* sigma_;
        const Tuple* tuple_;
        const Nat* type_nat_;
        const Def* table_id;
        const Def* table_not;
        std::array<const Lit*, 2> lit_bool_;
        // clang-format off
        std::array<const Axiom*, Num<Bit  >> Bit_;
        std::array<const Axiom*, Num<Shr  >> Shr_;
        std::array<const Axiom*, Num<Wrap >> Wrap_;
        std::array<const Axiom*, Num<Div  >> Div_;
        std::array<const Axiom*, Num<ROp  >> ROp_;
        std::array<const Axiom*, Num<ICmp >> ICmp_;
        std::array<const Axiom*, Num<RCmp >> RCmp_;
        std::array<const Axiom*, Num<Trait>> Trait_;
        std::array<const Axiom*, Num<Conv >> Conv_;
        std::array<const Axiom*, Num<PE   >> PE_;
        std::array<const Axiom*, Num<Acc  >> Acc_;
        // clang-format on
        const Lit* lit_nat_0_;
        const Lit* lit_nat_1_;
        const Lit* lit_nat_max_;
        const Lit* lit_univ_0_;
        const Lit* lit_univ_1_;
        const Axiom* alloc_;
        const Axiom* atomic_;
        const Axiom* bitcast_;
        const Axiom* lea_;
        const Axiom* load_;
        const Axiom* malloc_;
        const Axiom* mslot_;
        const Axiom* remem_;
        const Axiom* slot_;
        const Axiom* store_;
        const Axiom* type_int_;
        const Axiom* type_mem_;
        const Axiom* type_ptr_;
        const Axiom* type_real_;
        const Axiom* zip_;
        const Axiom* for_;
        std::string name_;
        Externals externals_;
        Sea defs_;
        DefDefMap<DefArray> cache_;
    } data_;

    std::unique_ptr<Checker> checker_;
    std::unique_ptr<ErrorHandler> err_;
    std::shared_ptr<Stream> stream_;

    friend DefArray Def::reduce(const Def*);
};

// clang-format off
#define ELOG(...) log(thorin::LogLevel::Error,   thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#define WLOG(...) log(thorin::LogLevel::Warn,    thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#define ILOG(...) log(thorin::LogLevel::Info,    thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#define VLOG(...) log(thorin::LogLevel::Verbose, thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#ifndef NDEBUG
#define DLOG(...) log(thorin::LogLevel::Debug,   thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#else
#define DLOG(...) log()
#endif
// clang-format on

} // namespace thorin

#endif
