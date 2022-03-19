#include "thorin/def.h"

#include <algorithm>
#include <stack>

#include "thorin/rewrite.h"
#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

/*
 * constructors
 */

Def::Def(node_t node, const Def* type, Defs ops, fields_t fields, const Def* dbg)
    : fields_(fields)
    , node_(unsigned(node))
    , nom_(false)
    , var_(false)
    , dep_(Dep::Bot)
    , proxy_(0)
    , num_ops_(ops.size())
    , dbg_(dbg)
    , type_(type) {
    gid_ = world().next_gid();
    std::ranges::copy(ops, ops_ptr());

    if (node == Node::Space) {
        hash_ = murmur3(gid());
    } else {
        hash_ = type->gid();
        for (auto op : ops) hash_ = murmur3(hash_, u32(op->gid()));
        hash_ = murmur3(hash_, fields_);
        hash_ = murmur3_rest(hash_, u8(node));
        hash_ = murmur3_finalize(hash_, num_ops());
    }
}

Def::Def(node_t node, const Def* type, size_t num_ops, fields_t fields, const Def* dbg)
    : fields_(fields)
    , node_(node)
    , nom_(true)
    , var_(false)
    , dep_(Dep::Nom)
    , proxy_(0)
    , num_ops_(num_ops)
    , dbg_(dbg)
    , type_(type) {
    gid_  = world().next_gid();
    hash_ = murmur3(gid());
    std::fill_n(ops_ptr(), num_ops, nullptr);
    if (!type->no_dep()) type->uses_.emplace(this, -1);
}

Kind::Kind(World& world)
    : Def(Node, (const Def*)world.space(), Defs{}, 0, nullptr) {}

Nat::Nat(World& world)
    : Def(Node, world.kind(), Defs{}, 0, nullptr) {}

// clang-format off

/*
 * rebuild
 */

const Def* App    ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.app(o[0], o[1], dbg); }
const Def* Arr    ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.arr(o[0], o[1], dbg); }
const Def* Et     ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.et(t, o, dbg); }
const Def* Extract::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.extract_(t, o[0], o[1], dbg); }
const Def* Insert ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.insert(o[0], o[1], o[2], dbg); }
const Def* Kind   ::rebuild(World& w, const Def*  , Defs  , const Def*    ) const { return w.kind(); }
const Def* Lam    ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.lam(t->as<Pi>(), o[0], o[1], dbg); }
const Def* Lit    ::rebuild(World& w, const Def* t, Defs  , const Def* dbg) const { return w.lit(t, get(), dbg); }
const Def* Nat    ::rebuild(World& w, const Def*  , Defs  , const Def*    ) const { return w.type_nat(); }
const Def* Pack   ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.pack(t->arity(), o[0], dbg); }
const Def* Pi     ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.pi(o[0], o[1], dbg); }
const Def* Pick   ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.pick(t, o[0], dbg); }
const Def* Proxy  ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.proxy(t, o, as<Proxy>()->index(), as<Proxy>()->flags(), dbg); }
const Def* Sigma  ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.sigma(o, dbg); }
const Def* Space  ::rebuild(World& w, const Def*  , Defs  , const Def*    ) const { return w.space(); }
const Def* Test   ::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.test(o[0], o[1], o[2], o[3], dbg); }
const Def* Tuple  ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.tuple(t, o, dbg); }
const Def* Var    ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.var(t, o[0]->as_nom(), dbg); }
const Def* Vel    ::rebuild(World& w, const Def* t, Defs o, const Def* dbg) const { return w.vel(t, o[0], dbg); }

const Def* Axiom  ::rebuild(World& w, const Def* t, Defs  , const Def* dbg) const {
    auto res = w.axiom(normalizer(), t, tag(), flags(), dbg);
    assert(&w != &world() || gid() == res->gid());
    return res;
}

template<bool up> const Def* TExt  <up>::rebuild(World& w, const Def* t, Defs  , const Def* dbg) const { return w.ext  <up>(t,    dbg); }
template<bool up> const Def* TBound<up>::rebuild(World& w, const Def*  , Defs o, const Def* dbg) const { return w.bound<up>(   o, dbg); }

/*
 * stub
 */

Lam*    Lam   ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_lam  (t->as<Pi>(), cc(), dbg); }
Pi*     Pi    ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_pi   (t, dbg); }
Sigma*  Sigma ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_sigma(t, num_ops(), dbg); }
Arr*    Arr   ::stub(World& w, const Def* t, const Def* dbg) { return w.nom_arr  (t, shape(), dbg); }
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
    if (!is_free(var(), codom())) return world().pi(dom(), codom(), dbg());
    return nullptr;
}

const Def* Arr::restructure() {
    auto& w = world();
    if (auto n = isa_lit(shape()))
        return w.sigma(DefArray(*n, [&](size_t i) { return reduce(w.lit_int(*n, i)).back(); }));
    return nullptr;
}

/*
 * Def
 */

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
    if (isa<Space>()) return Defs();

    size_t offset = dbg() ? 2 : 1;
    return Defs((is_set() ? num_ops_ : 0) + offset, ops_ptr() - offset);
}

// clang-format off

const Var* Def::var(const Def* dbg) {
    auto& w = world();
    if (auto lam  = isa<Lam  >()) return w.var(lam ->dom(), lam, dbg);
    if (auto pi   = isa<Pi   >()) return w.var(pi  ->dom(),  pi, dbg);
    if (auto sig  = isa<Sigma>()) return w.var(sig,         sig, dbg);
    if (auto arr  = isa<Arr  >()) return w.var(w.type_int(arr ->shape()), arr,  dbg); // TODO shapes like (2, 3)
    if (auto pack = isa<Pack >()) return w.var(w.type_int(pack->shape()), pack, dbg); // TODO shapes like (2, 3)
    if (isa_bound(this)) return w.var(this, this,  dbg);
    unreachable();
}

Sort Def::level() const {
    if (                isa<Space>()) return Sort::Space;
    if (        type()->isa<Space>()) return Sort::Kind;
    if (type()->type()->isa<Space>()) return Sort::Type;
    return Sort::Term;
}

Sort Def::sort() const {
    switch (node()) {
        case Node::Space: return Sort::Space;
        case Node::Kind:  return Sort::Kind;
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
    if (auto sigma  = isa<Sigma>()) return world().lit_nat(sigma ->num_ops());
    if (auto arr    = isa<Arr  >()) return arr->shape();
    if (sort() == Sort::Term)       return type()->arity();
    return world().lit_nat(1);
}

// clang-format on

bool Def::equal(const Def* other) const {
    if (isa<Space>() || this->isa_nom() || other->isa_nom()) return this == other;

    bool result = this->node() == other->node() && this->fields() == other->fields() &&
                  this->num_ops() == other->num_ops() && this->type() == other->type();

    for (size_t i = 0, e = num_ops(); result && i != e; ++i) result &= this->op(i) == other->op(i);

    return result;
}

const Def* Def::debug_history() const {
#if THORIN_ENABLE_CHECKS
    auto& w = world();
    if (w.track_history())
        return dbg() ? w.insert(dbg(), 3_s, 0_s, w.tuple_str(unique_name())) : w.tuple_str(unique_name());
#endif
    return dbg();
}

#ifndef NDEBUG
void Def::set_debug_name(std::string_view n) const {
    auto& w   = world();
    auto name = w.tuple_str(n);

    if (dbg_ == nullptr) {
        auto file  = w.tuple_str("");
        auto begin = w.lit_nat_max();
        auto finis = w.lit_nat_max();
        auto meta  = w.bot(w.bot_kind());
        dbg_       = w.tuple({name, w.tuple({file, begin, finis}), meta});
    } else {
        dbg_ = w.insert(dbg_, 3_s, 0_s, name);
    }
}
#endif

void Def::finalize() {
    for (size_t i = 0, e = num_ops(); i != e; ++i) {
        if (auto dep = op(i)->dep(); dep != Dep::Bot) {
            dep_ |= dep;
            const auto& p = op(i)->uses_.emplace(this, i);
            assert_unused(p.second);
        }
    }

    if (!isa<Space>() && !isa<Axiom>()) {
        if (auto dep = type()->dep(); dep != Dep::Bot) {
            dep_ |= dep;
            const auto& p = type()->uses_.emplace(this, -1);
            assert_unused(p.second);
        }
    }

    assert(!dbg() || dbg()->no_dep());
    if (auto var = isa<Var>()) {
        var->nom()->var_ = true;
        dep_             = Dep::Var;
    }

    if (isa<Proxy>()) {
        proxy_ = true;
    } else {
        for (auto op : extended_ops()) proxy_ |= op->contains_proxy();
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

bool Def::is_set() const {
    if (!isa_nom()) {
        assert(std::ranges::all_of(ops(), [](auto op) { return op != nullptr; }) && "structurals must be always set");
        return true;
    }

    if (std::ranges::all_of(ops(), [](auto op) { return op != nullptr; })) return true;

    assert(std::ranges::all_of(ops(), [](auto op) { return op == nullptr; }) && "some operands are set, others aren't");
    return false;
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
    auto& cache = world().data_.cache_;
    if (auto res = cache.lookup({this, arg})) return *res;

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
    if (a == 1 && (!isa_nom<Sigma>() && !type()->isa_nom<Sigma>())) return this;

    if (isa<Tuple>() || isa<Sigma>()) {
        return op(i);
    } else if (auto arr = isa<Arr>()) {
        if (arr->arity()->isa<Top>()) return arr->body();
        return arr->reduce(world().lit_int(as_lit(arr->arity()), i)).back();
    } else if (auto pack = isa<Pack>()) {
        if (pack->arity()->isa<Top>()) return pack->body();
        return pack->reduce(world().lit_int(as_lit(pack->arity()), i)).back();
    } else if (sort() == Sort::Term) {
        return world().extract(this, a, i, dbg);
    }

    return nullptr;
}

/*
 * Global
 */

const App* Global::type() const { return thorin::as<Tag::Ptr>(Def::type()); }
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

} // namespace thorin
