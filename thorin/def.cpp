#include "thorin/def.h"

#include <algorithm>
#include <ranges>
#include <stack>

#include "thorin/rewrite.h"
#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

// Just assuming looking through the uses is faster if uses().size() is small.
static constexpr int Search_In_Uses_Threshold = 8;

/*
 * constructors
 */

Def::Def(World* w, node_t node, const Def* type, Defs ops, flags_t flags, const Def* dbg)
    : world_(w)
    , flags_(flags)
    , node_(unsigned(node))
    , nom_(false)
    , dep_(node == Node::Axiom   ? Dep::Axiom
           : node == Node::Proxy ? Dep::Proxy
           : node == Node::Var   ? Dep::Var
                                 : Dep::None)
    , num_ops_(ops.size())
    , dbg_(dbg)
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

Def::Def(node_t n, const Def* type, Defs ops, flags_t flags, const Def* dbg)
    : Def(nullptr, n, type, ops, flags, dbg) {}

Def::Def(node_t node, const Def* type, size_t num_ops, flags_t flags, const Def* dbg)
    : flags_(flags)
    , node_(node)
    , nom_(true)
    , dep_(Dep::Nom)
    , num_ops_(num_ops)
    , dbg_(dbg)
    , type_(type) {
    gid_  = world().next_gid();
    hash_ = murmur3(gid());
    std::fill_n(ops_ptr(), num_ops, nullptr);
    if (!type->dep_const()) type->uses_.emplace(this, Use::Type);
}

Nat::Nat(World& world)
    : Def(Node, world.type(), Defs{}, 0, nullptr) {}

UMax::UMax(World& world, Defs ops, const Def* dbg)
    : Def(Node, world.univ(), ops, 0, dbg) {}

// clang-format off

/*
 * rebuild
 */

const Def* Ac       ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.ac(t, o, dbg); }
const Def* App      ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.app(o[0], o[1], dbg); }
const Def* Arr      ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.arr(o[0], o[1], dbg); }
const Def* Extract  ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.extract(o[0], o[1], dbg); }
const Def* Insert   ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.insert(o[0], o[1], o[2], dbg); }
const Def* Idx      ::rebuild(World& w, const Def*  , Defs  , const Def*    ) const { return w.type_idx(); }
const Def* Lam      ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.lam(t->as<Pi>(), o[0], o[1], dbg); }
const Def* Lit      ::rebuild(World& w, const Def* t, Defs  , const Def* dbg) const { return w.lit(t, get(), dbg); }
const Def* Nat      ::rebuild(World& w, const Def*  , Defs  , const Def*    ) const { return w.type_nat(); }
const Def* Pack     ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.pack(t->arity(), o[0], dbg); }
const Def* Pi       ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.pi(o[0], o[1], dbg); }
const Def* Pick     ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.pick(t, o[0], dbg); }
const Def* Proxy    ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.proxy(t, o, as<Proxy>()->pass(), as<Proxy>()->tag(), dbg); }
const Def* Sigma    ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.sigma(o, dbg); }
const Def* Singleton::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.singleton(o[0], dbg); }
const Def* Type     ::rebuild(World& w, const Def*  , Defs o, const Def*    ) const { return w.type(o[0]); }
const Def* Test     ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.test(o[0], o[1], o[2], o[3], dbg); }
const Def* Tuple    ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.tuple(t, o, dbg); }
const Def* UInc     ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.uinc(o[0], offset(), dbg); }
const Def* UMax     ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.umax(o, dbg); }
const Def* Univ     ::rebuild(World& w, const Def*  , Defs  , const Def*    ) const { return w.univ(); }
const Def* Var      ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.var(t, o[0]->as_nom(), dbg); }
const Def* Vel      ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.vel(t, o[0], dbg); }

const Def* Axiom    ::rebuild(World& w, const Def* t, Defs  , const Def* dbg) const {
    if (&w != &world()) return w.axiom(normalizer(), curry(), trip(), t, dialect(), tag(), sub(), dbg);
    assert(w.checker().equiv(t, type(), dbg));
    return this;
}

template<bool up> const Def* TExt  <up>::rebuild(World& w, const Def* t, Defs  , const Def* dbg) const { return w.ext  <up>(t,    dbg); }
template<bool up> const Def* TBound<up>::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.bound<up>(   o, dbg); }

/*
 * stub
 */

Lam*    Lam   ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_lam  (t->as<Pi>(), dbg); }
Pi*     Pi    ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_pi   (t, dbg); }
Sigma*  Sigma ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_sigma(t, num_ops(), dbg); }
Arr*    Arr   ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_arr  (t, dbg); }
Pack*   Pack  ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_pack (t, dbg); }
Infer*  Infer ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_infer(t, dbg); }
Global* Global::stub(World& w, const Def* t, const Def* dbg) { return w.global(t, is_mutable(), dbg); }

// clang-format on

template<bool up>
TBound<up>* TBound<up>::stub(World& w, const Def* t, const Def* dbg) {
    return w.nom_bound<up>(t, num_ops(), dbg);
}

/*
 * restructure
 */

const Pi* Pi::restructure() {
    if (!is_free(this, codom())) return world().pi(dom(), codom(), dbg());
    return nullptr;
}

const Sigma* Sigma::restructure() {
    if (std::ranges::none_of(ops(), [this](auto op) { return is_free(this, op); }))
        return static_cast<const Sigma*>(world().sigma(ops(), dbg()));
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
    size_t offset = dbg() ? 2 : 1;
    return Defs((is_set() ? num_ops_ : 0) + offset, ops_ptr() - offset);
}

// clang-format off

const Var* Def::var(const Def* dbg) {
    auto& w = world();

    if (w.is_frozen() || uses().size() < Search_In_Uses_Threshold) {
        for (auto u : uses()) {
            if (auto var = u->isa<Var>(); var && var->nom() == this) return var;
        }

        if (w.is_frozen()) return nullptr;
    }

    if (auto lam  = isa<Lam  >()) return w.var(lam ->dom(), lam, dbg);
    if (auto pi   = isa<Pi   >()) return w.var(pi  ->dom(),  pi, dbg);
    if (auto sig  = isa<Sigma>()) return w.var(sig,         sig, dbg);
    if (auto arr  = isa<Arr  >()) return w.var(w.type_idx(arr ->shape()), arr,  dbg); // TODO shapes like (2, 3)
    if (auto pack = isa<Pack >()) return w.var(w.type_idx(pack->shape()), pack, dbg); // TODO shapes like (2, 3)
    if (isa<Bound >()) return w.var(this, this,  dbg);
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

#ifndef NDEBUG
void Def::set_debug_name(std::string_view n) const {
    auto& w   = world();
    auto name = w.tuple_str(n);

    if (dbg_ == nullptr) {
        auto file  = w.tuple_str("");
        auto begin = w.lit_nat_max();
        auto finis = w.lit_nat_max();
        auto meta  = w.bot(w.type_bot());
        dbg_       = w.tuple({name, w.tuple({file, begin, finis}), meta});
    } else {
        dbg_ = w.insert(dbg_, 3_s, 0_s, name);
    }
}
#endif

void Def::finalize() {
    assert(!dbg() || dbg()->dep_none());

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

std::string Def::unique_name() const { return name() + "_" + std::to_string(gid()); }

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
            def = callee != app->callee() ? world().app(callee, app->arg(), app->dbg()) : app;
            break;
        }
    }
    return def;
}

const Def* Def::refine(size_t i, const Def* new_op) const {
    DefArray new_ops(ops());
    new_ops[i] = new_op;
    return rebuild(world(), type(), new_ops, dbg());
}

const Def* Def::proj(nat_t a, nat_t i, const Def* dbg) const {
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

        return w.extract(this, a, i, dbg);
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

/*
 * instantiate templates
 */

// clang-format off

template const Def*     TExt  <false>::rebuild(World&, const Def*, Defs, const Def*) const;
template const Def*     TExt  <true >::rebuild(World&, const Def*, Defs, const Def*) const;
template const Def*     TBound<false>::rebuild(World&, const Def*, Defs, const Def*) const;
template const Def*     TBound<true >::rebuild(World&, const Def*, Defs, const Def*) const;
template TBound<false>* TBound<false>::stub(World&, const Def*, const Def*);
template TBound<true >* TBound<true >::stub(World&, const Def*, const Def*);

// clang-format on

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
