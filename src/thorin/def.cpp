#include "thorin/def.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <stack>

#include "thorin/rewrite.h"
#include "thorin/world.h"

#include "thorin/analyses/scope.h"

#include "fe/assert.h"

using namespace std::literals;

namespace thorin {

/*
 * constructors
 */

Def::Def(World* w, node_t node, const Def* type, Defs ops, flags_t flags)
    : world_(w)
    , flags_(flags)
    , node_(unsigned(node))
    , mut_(false)
    , external_(false)
    , dep_(unsigned(node == Node::Axiom   ? Dep::Axiom
                    : node == Node::Infer ? Dep::Infer
                    : node == Node::Proxy ? Dep::Proxy
                    : node == Node::Var   ? Dep::Var
                                          : Dep::None))
    , num_ops_(ops.size())
    , type_(type) {
    std::ranges::copy(ops, ops_ptr());
    gid_ = world().next_gid();

    if (auto var = isa<Var>()) {
        local_vars_.emplace(var);
    } else {
        for (auto op : extended_ops()) {
            local_vars_.insert(op->local_vars_.begin(), op->local_vars_.end());
            local_muts_.insert(op->local_muts_.begin(), op->local_muts_.end());
        }
    }

    if (node == Node::Univ) {
        hash_ = murmur3(gid());
    } else {
        hash_ = type ? type->gid() : 0;
        for (auto op : ops) hash_ = murmur3(hash_, u32(op->gid()));
        hash_ = murmur3(hash_, flags_);
        hash_ = murmur3_rest(hash_, u8(node));
        hash_ = murmur3_finalize(hash_, num_ops());
    }
}

Def::Def(node_t n, const Def* type, Defs ops, flags_t flags)
    : Def(nullptr, n, type, ops, flags) {}

Def::Def(node_t node, const Def* type, size_t num_ops, flags_t flags)
    : flags_(flags)
    , node_(node)
    , mut_(true)
    , external_(false)
    , dep_(Dep::Mut | (node == Node::Infer ? Dep::Infer : Dep::None))
    , num_ops_(num_ops)
    , type_(type) {
    gid_  = world().next_gid();
    hash_ = murmur3(gid());
    local_muts_.emplace(this);
    std::fill_n(ops_ptr(), num_ops, nullptr);
    if (!type->dep_const()) type->uses_.emplace(this, Use::Type);
}

Nat::Nat(World& world)
    : Def(Node, world.type(), Defs{}, 0) {}

UMax::UMax(World& world, Defs ops)
    : Def(Node, world.univ(), ops, 0) {}

const Var* Def::true_var() {
    if (auto v = var())
        if (auto w = v->isa<Var>()) return w;
    return nullptr;
}

VarSet Def::free_vars() const {
    if (auto mut = isa_mut()) return mut->free_vars();

    auto mut2vars = MutMap<VarSet>();
    auto vars     = local_vars();

    for (auto mut : local_muts()) {
        auto mut_fvs = mut->free_vars(mut2vars);
        vars.insert(mut_fvs.begin(), mut_fvs.end());
    }

    return vars;
}

VarSet Def::free_vars() {
    MutMap<VarSet> mut2vars;
    return free_vars(mut2vars);
}

VarSet Def::free_vars(MutMap<VarSet>& mut2vars) {
    if (auto [i, ins] = mut2vars.emplace(this, VarSet()); !ins) return i->second;

    VarSet vars;
    for (auto op : extended_ops()) {
        vars.insert(op->local_vars().begin(), op->local_vars().end());

        for (auto mut : op->local_muts()) {
            auto mut_fvs = mut->free_vars(mut2vars);
            vars.insert(mut_fvs.begin(), mut_fvs.end());
        }
    }

    if (isa_mut()) {
        if (auto var = true_var()) vars.erase(var);
    }

    return mut2vars[this] = vars;
}

// clang-format off

/// @name Rebuild
///@{
/*
 * rebuild
 */

#ifndef DOXYGEN // TODO Doxygen doesn't expand THORIN_DEF_MIXIN
Ref Infer    ::rebuild(World&,   Ref,   Defs  ) const { fe::unreachable(); }
Ref Global   ::rebuild(World&,   Ref,   Defs  ) const { fe::unreachable(); }
Ref Idx      ::rebuild(World& w, Ref  , Defs  ) const { return w.type_idx(); }
Ref Nat      ::rebuild(World& w, Ref  , Defs  ) const { return w.type_nat(); }
Ref Univ     ::rebuild(World& w, Ref  , Defs  ) const { return w.univ(); }
Ref Ac       ::rebuild(World& w, Ref t, Defs o) const { return w.ac(t, o)                     ->set(dbg()); }
Ref App      ::rebuild(World& w, Ref  , Defs o) const { return w.app(o[0], o[1])              ->set(dbg()); }
Ref Arr      ::rebuild(World& w, Ref  , Defs o) const { return w.arr(o[0], o[1])              ->set(dbg()); }
Ref Extract  ::rebuild(World& w, Ref  , Defs o) const { return w.extract(o[0], o[1])          ->set(dbg()); }
Ref Insert   ::rebuild(World& w, Ref  , Defs o) const { return w.insert(o[0], o[1], o[2])     ->set(dbg()); }
Ref Lam      ::rebuild(World& w, Ref t, Defs o) const { return w.lam(t->as<Pi>(), o[0], o[1]) ->set(dbg()); }
Ref Lit      ::rebuild(World& w, Ref t, Defs  ) const { return w.lit(t, get())                ->set(dbg()); }
Ref Pack     ::rebuild(World& w, Ref t, Defs o) const { return w.pack(t->arity(), o[0])       ->set(dbg()); }
Ref Pi       ::rebuild(World& w, Ref  , Defs o) const { return w.pi(o[0], o[1], is_implicit())->set(dbg()); }
Ref Pick     ::rebuild(World& w, Ref t, Defs o) const { return w.pick(t, o[0])                ->set(dbg()); }
Ref Proxy    ::rebuild(World& w, Ref t, Defs o) const { return w.proxy(t, o, pass(), tag())   ->set(dbg()); }
Ref Sigma    ::rebuild(World& w, Ref  , Defs o) const { return w.sigma(o)                     ->set(dbg()); }
Ref Singleton::rebuild(World& w, Ref  , Defs o) const { return w.singleton(o[0])              ->set(dbg()); }
Ref Type     ::rebuild(World& w, Ref  , Defs o) const { return w.type(o[0])                   ->set(dbg()); }
Ref Test     ::rebuild(World& w, Ref  , Defs o) const { return w.test(o[0], o[1], o[2], o[3]) ->set(dbg()); }
Ref Tuple    ::rebuild(World& w, Ref t, Defs o) const { return w.tuple(t, o)                  ->set(dbg()); }
Ref UInc     ::rebuild(World& w, Ref  , Defs o) const { return w.uinc(o[0], offset())         ->set(dbg()); }
Ref UMax     ::rebuild(World& w, Ref  , Defs o) const { return w.umax(o)                      ->set(dbg()); }
Ref Var      ::rebuild(World& w, Ref t, Defs o) const { return w.var(t, o[0]->as_mut())       ->set(dbg()); }
Ref Vel      ::rebuild(World& w, Ref t, Defs o) const { return w.vel(t, o[0])->set(dbg())     ->set(dbg()); }

Ref Axiom    ::rebuild(World& w, Ref t, Defs ) const {
    if (&w != &world()) return w.axiom(normalizer(), curry(), trip(), t, plugin(), tag(), sub())->set(dbg());
    assert(Check::alpha(t, type()));
    return this;
}

template<bool up> Ref TExt  <up>::rebuild(World& w, Ref t, Defs  ) const { return w.ext  <up>(t)->set(dbg()); }
template<bool up> Ref TBound<up>::rebuild(World& w, Ref  , Defs o) const { return w.bound<up>(o)->set(dbg()); }
#endif

/*
 * stub
 */

Arr*    Arr   ::stub(World& w, Ref t) { return w.mut_arr  (t)                ->set(dbg()); }
Global* Global::stub(World& w, Ref t) { return w.global   (t, is_mutable())  ->set(dbg()); }
Infer*  Infer ::stub(World& w, Ref t) { return w.mut_infer(t)                ->set(dbg()); }
Lam*    Lam   ::stub(World& w, Ref t) { return w.mut_lam  (t->as<Pi>())      ->set(dbg()); }
Pack*   Pack  ::stub(World& w, Ref t) { return w.mut_pack (t)                ->set(dbg()); }
Pi*     Pi    ::stub(World& w, Ref t) { return w.mut_pi   (t, is_implicit()) ->set(dbg()); }
Sigma*  Sigma ::stub(World& w, Ref t) { return w.mut_sigma(t, num_ops())     ->set(dbg()); }

template<bool up> TBound<up>* TBound<up>::stub(World& w, Ref t) { return w.mut_bound<up>(t, num_ops()); }
template<bool up> TExt  <up>* TExt  <up>::stub(World&  , Ref  ) { fe::unreachable(); }

/*
 * instantiate templates
 */

#ifndef DOXYGEN
template Ref            TExt<false>  ::rebuild(World&, Ref, Defs) const;
template Ref            TExt<true >  ::rebuild(World&, Ref, Defs) const;
template Ref            TBound<false>::rebuild(World&, Ref, Defs) const;
template Ref            TBound<true >::rebuild(World&, Ref, Defs) const;
template TBound<false>* TBound<false>::stub(World&, Ref);
template TBound<true >* TBound<true >::stub(World&, Ref);
template TExt<false>*   TExt<false>  ::stub(World&, Ref);
template TExt<true >*   TExt<true >  ::stub(World&, Ref);
#endif

// clang-format on

/*
 * immutabilize
 */

// TODO check for recursion
const Pi* Pi::immutabilize() {
    auto fvs = codom()->free_vars();
    if (auto v = true_var(); v && fvs.contains(v)) return nullptr;
    return world().pi(dom(), codom());
}

const Def* Sigma::immutabilize() {
    if (auto v = true_var(); v && std::ranges::any_of(ops(), [v](auto op) {
                                 auto fvs = op->free_vars();
                                 return fvs.contains(v);
                             }))
        return nullptr;
    return static_cast<const Sigma*>(*world().sigma(ops()));
}

const Def* Arr::immutabilize() {
    auto& w  = world();
    auto fvs = body()->free_vars();
    if (auto v = true_var(); !v || !fvs.contains(v)) return w.arr(shape(), body());

    if (auto n = Lit::isa(shape()); *n < w.flags().scalerize_threshold)
        return w.sigma(DefVec(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));

    return nullptr;
}

const Def* Pack::immutabilize() {
    auto& w  = world();
    auto fvs = body()->free_vars();
    if (auto v = true_var(); !v || !fvs.contains(v)) return w.pack(shape(), body());

    if (auto n = Lit::isa(shape()); *n < w.flags().scalerize_threshold)
        return w.tuple(DefVec(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));

    return nullptr;
}

/*
 * reduce
 */

const Def* Arr::reduce(const Def* arg) const {
    if (auto mut = isa_mut<Arr>()) return rewrite(mut, arg, 1);
    return body();
}

const Def* Pack::reduce(const Def* arg) const {
    if (auto mut = isa_mut<Pack>()) return rewrite(mut, arg, 0);
    return body();
}

///@}

DefVec Def::reduce(const Def* arg) const {
    if (auto mut = isa_mut()) return mut->reduce(arg);
    return DefVec(ops().begin(), ops().end());
}

DefVec Def::reduce(const Def* arg) {
    auto& cache = world().move_.cache;
    if (auto i = cache.find({this, arg}); i != cache.end()) return i->second;

    return cache[{this, arg}] = rewrite(this, arg);
}

const Def* Def::refine(size_t i, const Def* new_op) const {
    DefVec new_ops(ops().begin(), ops().end());
    new_ops[i] = new_op;
    return rebuild(world(), type(), new_ops);
}

/*
 * Def
 */

Sym Def::sym(const char* s) const { return world().sym(s); }
Sym Def::sym(std::string_view s) const { return world().sym(s); }
Sym Def::sym(std::string s) const { return world().sym(std::move(s)); }

World& Def::world() const {
    if (isa<Univ>()) return *world_;
    if (auto type = isa<Type>()) return type->level()->world();
    return type()->world(); // TODO unroll
}

const Def* Def::unfold_type() const {
    if (!type_) {
        if (auto t = isa<Type>()) return world().type(world().uinc(t->level()));
        assert(isa<Univ>());
        return nullptr;
    }

    return type_;
}

std::string_view Def::node_name() const {
    switch (node()) {
#define CODE(op, abbr) \
    case Node::op: return #abbr;
        THORIN_NODE(CODE)
#undef CODE
        default: fe::unreachable();
    }
}

Defs Def::extended_ops() const {
    if (isa<Type>() || isa<Univ>()) return Defs();
    assert(type());
    return Defs(ops_ptr() - 1, (is_set() ? num_ops_ : 0) + 1);
}

#ifndef NDEBUG
const Def* Def::debug_prefix(std::string prefix) const {
    dbg_.sym = world().sym(prefix + sym().str());
    return this;
}

const Def* Def::debug_suffix(std::string suffix) const {
    dbg_.sym = world().sym(sym().str() + suffix);
    return this;
}
#endif

// clang-format off

Ref Def::var() {
    if (var_) return var_;
    auto& w = world();

    if (w.is_frozen()) return nullptr;
    if (auto lam  = isa<Lam  >()) return w.var(lam ->dom(), lam);
    if (auto pi   = isa<Pi   >()) return w.var(pi  ->dom(),  pi);
    if (auto sig  = isa<Sigma>()) return w.var(sig,         sig);
    if (auto arr  = isa<Arr  >()) return w.var(w.type_idx(arr ->shape()), arr ); // TODO shapes like (2, 3)
    if (auto pack = isa<Pack >()) return w.var(w.type_idx(pack->shape()), pack); // TODO shapes like (2, 3)
    if (isa<Bound >()) return w.var(this, this);
    if (isa<Infer >()) return nullptr;
    if (isa<Global>()) return nullptr;
    fe::unreachable();
}

bool Def::is_term() const {
    if (auto t = type()) {
        if (auto u = t->type()) {
            if (auto type = u->isa<Type>()) {
                if (auto level = Lit::isa(type->level())) return *level == 0;
            }
        }
    }
    return false;
}

Ref Def::arity() const {
    if (auto sigma  = isa<Sigma>()) return world().lit_nat(sigma->num_ops());
    if (auto arr    = isa<Arr  >()) return arr->shape();
    if (auto t = type())            return t->arity();
    return world().lit_nat_1();
}

std::optional<nat_t> Def::isa_lit_arity() const {
    if (auto sigma  = isa<Sigma>()) return sigma->num_ops();
    if (auto arr    = isa<Arr  >()) return Lit::isa(arr->shape());
    if (auto t = type())            return t->isa_lit_arity();
    return 1;
}

// clang-format on

bool Def::equal(const Def* other) const {
    if (isa<Univ>() || this->isa_mut() || other->isa_mut()) return this == other;

    bool result = this->node() == other->node() && this->flags() == other->flags()
               && this->num_ops() == other->num_ops() && this->type() == other->type();

    for (size_t i = 0, e = num_ops(); result && i != e; ++i) result &= this->op(i) == other->op(i);

    return result;
}

void Def::finalize() {
    for (size_t i = Use::Type; auto op : partial_ops()) {
        if (op) {
            dep_ |= op->dep();
            if (!op->dep_const()) {
                const auto& p = op->uses_.emplace(this, i);
                assert_unused(p.second);
            }
        }
        ++i;
    }
}

// clang-format off
Def* Def::  set(Defs ops) { assert(ops.size() == num_ops()); for (size_t i = 0, e = num_ops(); i != e; ++i)   set(i, ops[i]); return this; }
Def* Def::reset(Defs ops) { assert(ops.size() == num_ops()); for (size_t i = 0, e = num_ops(); i != e; ++i) reset(i, ops[i]); return this; }
// clang-format on

Def* Def::set(size_t i, const Def* def) {
    assert(def && !op(i) && curr_op_ == i);
#ifndef NDEBUG
    curr_op_ = (curr_op_ + 1) % num_ops();
#endif
    ops_ptr()[i]  = def;
    const auto& p = def->uses_.emplace(this, i);
    assert_unused(p.second);

    if (i == num_ops() - 1) {
        check();
        update();
    }

    return this;
}

Def* Def::unset() {
#ifndef NDEBUG
    curr_op_ = 0;
#endif
    for (size_t i = 0, e = num_ops(); i != e; ++i) {
        if (op(i))
            unset(i);
        else {
            assert(std::all_of(ops_ptr() + i + 1, ops_ptr() + num_ops(), [](auto op) { return !op; }));
            break;
        }
    }
    return this;
}

Def* Def::unset(size_t i) {
    assert(op(i) && op(i)->uses_.contains(Use(this, i)));
    op(i)->uses_.erase(Use(this, i));
    ops_ptr()[i] = nullptr;
    return this;
}

Def* Def::set_type(const Def* type) {
    if (type_ != nullptr) unset_type();
    type_ = type;
    type->uses_.emplace(this, Use::Type);
    return this;
}

void Def::unset_type() {
    assert(type_->uses_.contains(Use(this, Use::Type)));
    type_->uses_.erase(Use(this, Use::Type));
    type_ = nullptr;
}

bool Def::is_set() const {
    if (num_ops() == 0) return true;
    bool result = ops().back();
    assert((!result || std::ranges::all_of(ops().rsubspan(1), [](auto op) { return op; }))
           && "the last operand is set but others in front of it aren't");
    return result;
}

void Def::make_external() { return world().make_external(this); }
void Def::make_internal() { return world().make_internal(this); }

std::string Def::unique_name() const { return sym().str() + "_"s + std::to_string(gid()); }

nat_t Def::num_tprojs() const {
    if (auto a = isa_lit_arity(); a && *a < world().flags().scalerize_threshold) return *a;
    return 1;
}

const Def* Def::proj(nat_t a, nat_t i) const {
    static constexpr int Search_In_Uses_Threshold = 8;

    if (a == 1) {
        if (!type()) return this;
        if (!isa_mut<Sigma>() && !type()->isa_mut<Sigma>()) return this;
    }

    World& w = world();

    if (isa<Tuple>() || isa<Sigma>()) {
        return op(i);
    } else if (auto arr = isa<Arr>()) {
        if (arr->arity()->isa<Top>()) return arr->body();
        return arr->reduce(w.lit_idx(a, i));
    } else if (auto pack = isa<Pack>()) {
        if (pack->arity()->isa<Top>()) return pack->body();
        assert(!w.is_frozen() && "TODO");
        return pack->reduce(w.lit_idx(a, i));
    }

    if (w.is_frozen() || uses().size() < Search_In_Uses_Threshold) {
        for (auto u : uses()) {
            if (auto ex = u->isa<Extract>(); ex && ex->tuple() == this) {
                if (auto index = Lit::isa(ex->index()); index && *index == i) return ex;
            }
        }

        if (w.is_frozen()) return nullptr;
    }

    return w.extract(this, a, i);
}

/*
 * Idx
 */

Ref Idx::size(Ref def) {
    if (auto app = def->isa<App>()) {
        if (app->callee()->isa<Idx>()) return app->arg();
    }

    return nullptr;
}

std::optional<nat_t> Idx::size2bitwidth(const Def* size) {
    if (size->isa<Top>()) return 64;
    if (auto s = Lit::isa(size)) return size2bitwidth(*s);
    return {};
}

/*
 * Global
 */

const App* Global::type() const { return Def::type()->as<App>(); }
const Def* Global::alloced_type() const { return type()->arg(0); }

} // namespace thorin
