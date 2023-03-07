#include "thorin/def.h"

#include <algorithm>
#include <ranges>
#include <stack>

#include "thorin/rewrite.h"
#include "thorin/world.h"

#include "thorin/analyses/scope.h"

using namespace std::literals;

namespace thorin {

// Just assuming looking through the uses is faster if uses().size() is small.
static constexpr int Search_In_Uses_Threshold = 8;

/*
 * constructors
 */

Def::Def(World* w, node_t node, const Def* type, Defs ops, flags_t flags)
    : world_(w)
    , flags_(flags)
    , node_(unsigned(node))
    , nom_(false)
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
    , nom_(true)
    , dep_(Dep::Nom | (node == Node::Infer ? Dep::Infer : Dep::None))
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

/*
 * rebuild
 */

const Def* Infer    ::rebuild_(World&,   const Def*,   Defs  ) const { unreachable(); }
const Def* Global   ::rebuild_(World&,   const Def*,   Defs  ) const { unreachable(); }
const Def* Ac       ::rebuild_(World& w, const Def* t, Defs o) const { return w.ac(t, o); }
const Def* App      ::rebuild_(World& w, const Def*  , Defs o) const { return w.app(o[0], o[1]); }
const Def* Arr      ::rebuild_(World& w, const Def*  , Defs o) const { return w.arr(o[0], o[1]); }
const Def* Extract  ::rebuild_(World& w, const Def*  , Defs o) const { return w.extract(o[0], o[1]); }
const Def* Insert   ::rebuild_(World& w, const Def*  , Defs o) const { return w.insert(o[0], o[1], o[2]); }
const Def* Idx      ::rebuild_(World& w, const Def*  , Defs  ) const { return w.type_idx(); }
const Def* Lam      ::rebuild_(World& w, const Def* t, Defs o) const { return w.lam(t->as<Pi>(), o[0], o[1]); }
const Def* Lit      ::rebuild_(World& w, const Def* t, Defs  ) const { return w.lit(t, get()); }
const Def* Nat      ::rebuild_(World& w, const Def*  , Defs  ) const { return w.type_nat(); }
const Def* Pack     ::rebuild_(World& w, const Def* t, Defs o) const { return w.pack(t->arity(), o[0]); }
const Def* Pi       ::rebuild_(World& w, const Def*  , Defs o) const { return w.pi(o[0], o[1], implicit()); }
const Def* Pick     ::rebuild_(World& w, const Def* t, Defs o) const { return w.pick(t, o[0]); }
const Def* Proxy    ::rebuild_(World& w, const Def* t, Defs o) const { return w.proxy(t, o, as<Proxy>()->pass(), as<Proxy>()->tag()); }
const Def* Sigma    ::rebuild_(World& w, const Def*  , Defs o) const { return w.sigma(o); }
const Def* Singleton::rebuild_(World& w, const Def*  , Defs o) const { return w.singleton(o[0]); }
const Def* Type     ::rebuild_(World& w, const Def*  , Defs o) const { return w.type(o[0]); }
const Def* Test     ::rebuild_(World& w, const Def*  , Defs o) const { return w.test(o[0], o[1], o[2], o[3]); }
const Def* Tuple    ::rebuild_(World& w, const Def* t, Defs o) const { return w.tuple(t, o); }
const Def* UInc     ::rebuild_(World& w, const Def*  , Defs o) const { return w.uinc(o[0], offset()); }
const Def* UMax     ::rebuild_(World& w, const Def*  , Defs o) const { return w.umax(o); }
const Def* Univ     ::rebuild_(World& w, const Def*  , Defs  ) const { return w.univ(); }
const Def* Var      ::rebuild_(World& w, const Def* t, Defs o) const { return w.var(t, o[0]->as_nom()); }
const Def* Vel      ::rebuild_(World& w, const Def* t, Defs o) const { return w.vel(t, o[0]); }

const Def* Axiom    ::rebuild_(World& w, const Def* t, Defs ) const {
    if (&w != &world()) return w.axiom(normalizer(), curry(), trip(), t, dialect(), tag(), sub());
    assert(w.checker().equiv(t, type()));
    return this;
}

template<bool up> const Def* TExt  <up>::rebuild_(World& w, const Def* t, Defs  ) const { return w.ext  <up>(t); }
template<bool up> const Def* TBound<up>::rebuild_(World& w, const Def*  , Defs o) const { return w.bound<up>(o); }

/*
 * stub
 */

Lam*    Lam   ::stub_(World& w, const Def* t) { return w.nom_lam  (t->as<Pi>()); }
Pi*     Pi    ::stub_(World& w, const Def* t) { return w.nom_pi   (t, implicit()); }
Sigma*  Sigma ::stub_(World& w, const Def* t) { return w.nom_sigma(t, num_ops()); }
Arr*    Arr   ::stub_(World& w, const Def* t) { return w.nom_arr  (t); }
Pack*   Pack  ::stub_(World& w, const Def* t) { return w.nom_pack (t); }
Infer*  Infer ::stub_(World& w, const Def* t) { return w.nom_infer(t); }
Global* Global::stub_(World& w, const Def* t) { return w.global(t, is_mutable()); }

// clang-format on

template<bool up>
TBound<up>* TBound<up>::stub_(World& w, const Def* t) {
    return w.nom_bound<up>(t, num_ops());
}

// instantiate templates

// clang-format off
template const Def*     TExt  <false>::rebuild_(World&, const Def*, Defs) const;
template const Def*     TExt  <true >::rebuild_(World&, const Def*, Defs) const;
template const Def*     TBound<false>::rebuild_(World&, const Def*, Defs) const;
template const Def*     TBound<true >::rebuild_(World&, const Def*, Defs) const;
template TBound<false>* TBound<false>::stub_(World&, const Def*);
template TBound<true >* TBound<true >::stub_(World&, const Def*);
// clang-format on

/*
 * restructure
 */

const Pi* Pi::restructure() {
    if (!is_free(this, codom())) return world().pi(dom(), codom());
    return nullptr;
}

const Sigma* Sigma::restructure() {
    if (std::ranges::none_of(ops(), [this](auto op) { return is_free(this, op); }))
        return static_cast<const Sigma*>(*world().sigma(ops()));
    return nullptr;
}

const Def* Arr::restructure() {
    auto& w = world();
    if (auto n = isa_lit(shape())) {
        if (is_free(this, body())) return w.sigma(DefArray(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));
        return w.arr(shape(), body());
    }
    return nullptr;
}

const Def* Pack::restructure() {
    auto& w = world();
    if (auto n = isa_lit(shape())) {
        if (is_free(this, body())) return w.tuple(DefArray(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));
        return w.pack(shape(), body());
    }
    return nullptr;
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
            if (auto var = u->isa<Var>(); var && var->nom() == this) return var;
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

Sort Def::sort() const {
    if (auto type = isa<Type>()) {
        auto level = as_lit(type->level()); // TODO other levels
        return level == 0 ? Sort::Kind : Sort::Space;
    }

    switch (node()) {
        case Node::Univ:  return Sort::Univ;
        case Node::Arr:
        case Node::Nat:
        case Node::Pi:
        case Node::Sigma:
        case Node::Join:
        case Node::Meet: return Sort::Type;
        case Node::Global:
        case Node::Insert:
        case Node::Lam:
        case Node::Pack:
        case Node::Test:
        case Node::Tuple: return Sort::Term;
        default:          return Sort(int(type()->sort()) - 1);
    }
}

const Def* Def::arity() const {
    if (auto sigma  = isa<Sigma>()) return world().lit_nat(sigma->num_ops());
    if (auto arr    = isa<Arr  >()) return arr->shape();
    if (sort() == Sort::Term)       return type()->arity();
    return world().lit_nat(1);
}

// clang-format on

bool Def::equal(const Def* other) const {
    if (isa<Univ>() || this->isa_nom() || other->isa_nom()) return this == other;

    bool result = this->node() == other->node() && this->flags() == other->flags() &&
                  this->num_ops() == other->num_ops() && this->type() == other->type();

    for (size_t i = 0, e = num_ops(); result && i != e; ++i) result &= this->op(i) == other->op(i);

    return result;
}

void Def::finalize() {
    for (size_t i = 0, e = num_ops(); i != e; ++i) {
        dep_ |= op(i)->dep();
        if (!op(i)->dep_const()) {
            const auto& p = op(i)->uses_.emplace(this, i);
            assert_unused(p.second);
        }
    }

    if (auto t = type()) {
        dep_ |= t->dep();
        if (!t->dep_const()) {
            const auto& p = type()->uses_.emplace(this, Use::Type);
            assert_unused(p.second);
        }
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
bool Def::is_external() const { return world().is_external(this); }

std::string Def::unique_name() const { return *sym() + "_"s + std::to_string(gid()); }

DefArray Def::reduce(const Def* arg) const {
    if (auto nom = isa_nom()) return nom->reduce(arg);
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
        if (callee->isa_nom()) {
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

const Def* Def::proj(nat_t a, nat_t i) const {
    if (a == 1) {
        if (!type()) return this;
        if (!isa_nom<Sigma>() && !type()->isa_nom<Sigma>()) return this;
    }

    World& w = world();

    if (isa<Tuple>() || isa<Sigma>()) {
        return op(i);
    } else if (auto arr = isa<Arr>()) {
        if (arr->arity()->isa<Top>()) return arr->body();
        return arr->reduce(w.lit_idx(as_lit(arr->arity()), i));
    } else if (auto pack = isa<Pack>()) {
        if (pack->arity()->isa<Top>()) return pack->body();
        assert(!w.is_frozen() && "TODO");
        return pack->reduce(w.lit_idx(as_lit(pack->arity()), i));
    } else if (sort() == Sort::Term) {
        if (w.is_frozen() || uses().size() < Search_In_Uses_Threshold) {
            for (auto u : uses()) {
                if (auto ex = u->isa<Extract>(); ex && ex->tuple() == this) {
                    if (auto index = isa_lit(ex->index()); index && *index == i) return ex;
                }
            }

            if (w.is_frozen()) return nullptr;
        }

        return w.extract(this, a, i);
    }

    return nullptr;
}

/*
 * Idx
 */

const Def* Idx::size(Ref def) {
    if (auto app = def->isa<App>()) {
        if (app->callee()->isa<Idx>()) return app->arg();
    }

    return nullptr;
}

std::optional<nat_t> Idx::size2bitwidth(const Def* size) {
    if (size->isa<Top>()) return 64;
    if (auto s = isa_lit(size)) return size2bitwidth(*s);
    return {};
}

/*
 * Global
 */

const App* Global::type() const { return Def::type()->as<App>(); }
const Def* Global::alloced_type() const { return type()->arg(0); }

std::pair<const Def*, std::vector<const Def*>> collect_args(const Def* def) {
    std::vector<const Def*> args;
    if (auto app = def->isa<App>()) {
        auto callee               = app->callee();
        auto arg                  = app->arg();
        auto [inner_callee, args] = collect_args(callee);
        args.push_back(arg);
        return {inner_callee, args};
    } else {
        return {def, args};
    }
}

} // namespace thorin
