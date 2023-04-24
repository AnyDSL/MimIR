#include "thorin/def.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <stack>

#include "thorin/rewrite.h"
#include "thorin/world.h"

#include "thorin/analyses/scope.h"

using namespace std::literals;

namespace thorin {

namespace {
// Just assuming looking through the uses is faster if uses().size() is small.
constexpr int Search_In_Uses_Threshold = 8;
} // namespace

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
    std::fill_n(ops_ptr(), num_ops, nullptr);
    if (!type->dep_const()) type->uses_.emplace(this, Use::Type);
}

Nat::Nat(World& world)
    : Def(Node, world.type(), Defs{}, 0) {}

UMax::UMax(World& world, Defs ops)
    : Def(Node, world.univ(), ops, 0) {}

// clang-format off

/// @name Rebuild
///@{
/*
 * rebuild
 */

Ref Infer    ::rebuild(World&,   Ref,   Defs  ) const { unreachable(); }
Ref Global   ::rebuild(World&,   Ref,   Defs  ) const { unreachable(); }
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
    assert(w.checker().equiv(t, type()));
    return this;
}

template<bool up> Ref TExt  <up>::rebuild(World& w, Ref t, Defs  ) const { return w.ext  <up>(t)->set(dbg()); }
template<bool up> Ref TBound<up>::rebuild(World& w, Ref  , Defs o) const { return w.bound<up>(o)->set(dbg()); }

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
template<bool up> TExt  <up>* TExt  <up>::stub(World&  , Ref  ) { unreachable(); }

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
    if (!Scope::is_free(this, codom())) return world().pi(dom(), codom());
    return nullptr;
}

const Sigma* Sigma::immutabilize() {
    if (std::ranges::none_of(ops(), [this](auto op) { return Scope::is_free(this, op); }))
        return static_cast<const Sigma*>(*world().sigma(ops()));
    return nullptr;
}

const Def* Arr::immutabilize() {
    auto& w = world();
    if (auto n = Lit::isa(shape())) {
        if (Scope::is_free(this, body()))
            return w.sigma(DefArray(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));
        return w.arr(shape(), body());
    }
    return nullptr;
}

const Def* Pack::immutabilize() {
    auto& w = world();
    if (auto n = Lit::isa(shape())) {
        if (Scope::is_free(this, body()))
            return w.tuple(DefArray(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));
        return w.pack(shape(), body());
    }
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

DefArray Def::reduce(const Def* arg) const {
    if (auto mut = isa_mut()) return mut->reduce(arg);
    return ops();
}

DefArray Def::reduce(const Def* arg) {
    auto& cache = world().move_.cache;
    if (auto i = cache.find({this, arg}); i != cache.end()) return i->second;

    return cache[{this, arg}] = rewrite(this, arg);
}

const Def* Def::reduce_rec() const {
    auto def = this;
    while (auto app = def->isa<App>()) {
        auto callee = app->callee()->reduce_rec();
        if (callee->isa_mut()) {
            def = callee->reduce(app->arg()).back();
        } else {
            def = callee != app->callee() ? world().app(callee, app->arg()) : app;
            break;
        }
    }
    return def;
}

const Def* Def::refine(size_t i, const Def* new_op) const {
    DefArray new_ops(ops());
    new_ops[i] = new_op;
    return rebuild(world(), type(), new_ops);
}

/*
 * Def
 */

Sym Def::get_sym(const char* s) const { return world().sym(s); }
Sym Def::get_sym(std::string_view s) const { return world().sym(s); }
Sym Def::get_sym(std::string s) const { return world().sym(std::move(s)); }

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

    return type_->reduce_rec();
}

std::string_view Def::node_name() const {
    switch (node()) {
#define CODE(op, abbr) \
    case Node::op: return #abbr;
        THORIN_NODE(CODE)
#undef CODE
        default: unreachable();
    }
}

Defs Def::extended_ops() const {
    if (isa<Type>() || isa<Univ>()) return Defs();
    assert(type());
    return Defs((is_set() ? num_ops_ : 0) + 1, ops_ptr() - 1);
}

#ifndef NDEBUG
const Def* Def::debug_prefix(std::string prefix) const {
    dbg_.sym = world().sym(prefix + *sym());
    return this;
}

const Def* Def::debug_suffix(std::string suffix) const {
    dbg_.sym = world().sym(*sym() + suffix);
    return this;
}
#endif

// clang-format off

const Var* Def::var() {
    auto& w = world();

    if (w.is_frozen() || uses().size() < Search_In_Uses_Threshold) {
        for (auto u : uses()) {
            if (auto var = u->isa<Var>(); var && var->mut() == this) return var;
        }

        if (w.is_frozen()) return nullptr;
    }

    if (auto lam  = isa<Lam  >()) return w.var(lam ->dom(), lam);
    if (auto pi   = isa<Pi   >()) return w.var(pi  ->dom(),  pi);
    if (auto sig  = isa<Sigma>()) return w.var(sig,         sig);
    if (auto arr  = isa<Arr  >()) return w.var(w.type_idx(arr ->shape()), arr ); // TODO shapes like (2, 3)
    if (auto pack = isa<Pack >()) return w.var(w.type_idx(pack->shape()), pack); // TODO shapes like (2, 3)
    if (isa<Bound >()) return w.var(this, this);
    if (isa<Infer >()) return nullptr;
    if (isa<Global>()) return nullptr;
    unreachable();
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

const Def* Def::arity() const {
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

    bool result = this->node() == other->node() && this->flags() == other->flags() &&
                  this->num_ops() == other->num_ops() && this->type() == other->type();

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

Def* Def::set(size_t i, const Def* def) {
    if (op(i) == def) return this;
    if (op(i) != nullptr) unset(i);

    if (def != nullptr) {
        assert(i < num_ops() && "index out of bounds");
        ops_ptr()[i]  = def;
        const auto& p = def->uses_.emplace(this, i);
        assert_unused(p.second);

        // TODO check that others are set
        if (i == num_ops() - 1) {
            check();
            update();
        }
    } else {
        world().WLOG("You shouldn't invoke with nullptr: 'Def::set({}, nullptr)'", i);
    }
    return this;
}

void Def::unset(size_t i) {
    assert(i < num_ops() && "index out of bounds");
    auto def = op(i);
    assert(def->uses_.contains(Use(this, i)));
    def->uses_.erase(Use(this, i));
    assert(!def->uses_.contains(Use(this, i)));
    ops_ptr()[i] = nullptr;
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
    assert(!type_->uses_.contains(Use(this, Use::Type)));
    type_ = nullptr;
}

bool Def::is_set() const {
    if (num_ops() == 0) return true;
    bool result = ops().back();
    assert((!result || std::ranges::all_of(ops().skip_back(), [](auto op) { return op; })) &&
           "the last operand is set but others in front of it aren't");
    return result;
}

void Def::make_external() { return world().make_external(this); }
void Def::make_internal() { return world().make_internal(this); }

std::string Def::unique_name() const { return *sym() + "_"s + std::to_string(gid()); }

const Def* Def::proj(nat_t a, nat_t i) const {
    if (a == 1) {
        if (!type()) return this;
        if (!isa_mut<Sigma>() && !type()->isa_mut<Sigma>()) return this;
    }

    World& w = world();

    if (isa<Tuple>() || isa<Sigma>()) {
        return op(i);
    } else if (auto arr = isa<Arr>()) {
        if (arr->arity()->isa<Top>()) return arr->body();
        return arr->reduce(w.lit_idx(arr->as_lit_arity(), i));
    } else if (auto pack = isa<Pack>()) {
        if (pack->arity()->isa<Top>()) return pack->body();
        assert(!w.is_frozen() && "TODO");
        return pack->reduce(w.lit_idx(pack->as_lit_arity(), i));
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
