#include "mim/def.h"

#include <algorithm>

#include <absl/container/fixed_array.h>
#include <fe/assert.h>

#include "mim/world.h"

#include "mim/util/hash.h"

using namespace std::literals;

namespace mim {

template void Sets<const Var>::dot();
template void Sets<Def>::dot();

/*
 * constructors
 */

Def::Def(World* world, Node node, const Def* type, Defs ops, flags_t flags)
    : world_(world)
    , flags_(flags)
    , node_(node)
    , mut_(false)
    , external_(false)
    , dep_(node == Node::Hole    ? unsigned(Dep::Hole)
           : node == Node::Proxy ? unsigned(Dep::Proxy)
           : node == Node::Var   ? (Dep::Var | Dep::Mut)
                                 : 0)
    , num_ops_(ops.size())
    , type_(type) {
    if (node == Node::Univ) {
        gid_  = world->next_gid();
        hash_ = mim::hash_begin(node_t(Node::Univ));
    } else if (auto var = isa<Var>()) {
        assert(flags_ == 0); // if we ever need flags here, we need to hash that
        auto& world = type->world();
        gid_        = world.next_gid();
        vars_       = world.vars().insert(type->local_vars(), var);
        muts_       = type->local_muts();
        dep_ |= type->dep_;
        auto op      = ops[0];
        ops_ptr()[0] = op;
        hash_        = hash_begin(node_t(Node::Var));
        hash_        = hash_combine(hash_, type->gid());
        hash_        = hash_combine(hash_, op->gid());
    } else {
        hash_ = hash_begin(u8(node));
        hash_ = hash_combine(hash_, flags_);

        if (type) {
            world = &type->world();
            dep_ |= type->dep_;
            vars_ = type->local_vars();
            muts_ = type->local_muts();
            hash_ = hash_combine(hash_, type->gid());
        } else {
            world = &ops[0]->world();
        }

        auto vars = &world->vars();
        auto muts = &world->muts();
        auto ptr  = ops_ptr();
        gid_      = world->next_gid();

        for (size_t i = 0, e = ops.size(); i != e; ++i) {
            auto op = ops[i];
            ptr[i]  = op;
            dep_ |= op->dep_;
            vars_ = vars->merge(vars_, op->local_vars());
            muts_ = muts->merge(muts_, op->local_muts());
            hash_ = hash_combine(hash_, op->gid());
        }
    }
}

Def::Def(Node n, const Def* type, Defs ops, flags_t flags)
    : Def(nullptr, n, type, ops, flags) {}

Def::Def(Node node, const Def* type, size_t num_ops, flags_t flags)
    : flags_(flags)
    , node_(node)
    , mut_(true)
    , external_(false)
    , dep_(Dep::Mut | (node == Node::Hole ? Dep::Hole : Dep::None))
    , num_ops_(num_ops)
    , type_(type) {
    gid_  = world().next_gid();
    hash_ = mim::hash(gid());
    var_  = nullptr;
    std::fill_n(ops_ptr(), num_ops, nullptr);
}

Nat::Nat(World& world)
    : Def(Node, world.type(), Defs{}, 0) {}

UMax::UMax(World& world, Defs ops)
    : Def(Node, world.univ(), ops, 0) {}

// clang-format off

/*
 * rebuild
 */

const Def* Hole   ::rebuild_(World&,   const Def*,   Defs  ) const { fe::unreachable(); }
const Def* Global ::rebuild_(World&,   const Def*,   Defs  ) const { fe::unreachable(); }
const Def* Idx    ::rebuild_(World& w, const Def*  , Defs  ) const { return w.type_idx(); }
const Def* Nat    ::rebuild_(World& w, const Def*  , Defs  ) const { return w.type_nat(); }
const Def* Univ   ::rebuild_(World& w, const Def*  , Defs  ) const { return w.univ(); }
const Def* App    ::rebuild_(World& w, const Def*  , Defs o) const { return w.app(o[0], o[1]); }
const Def* Arr    ::rebuild_(World& w, const Def*  , Defs o) const { return w.arr(o[0], o[1]); }
const Def* Extract::rebuild_(World& w, const Def*  , Defs o) const { return w.extract(o[0], o[1]); }
const Def* Inj    ::rebuild_(World& w, const Def* t, Defs o) const { return w.inj(t, o[0])->set(dbg()); }
const Def* Insert ::rebuild_(World& w, const Def*  , Defs o) const { return w.insert(o[0], o[1], o[2]); }
const Def* Lam    ::rebuild_(World& w, const Def* t, Defs o) const { return w.lam(t->as<Pi>(), o[0], o[1]); }
const Def* Lit    ::rebuild_(World& w, const Def* t, Defs  ) const { return w.lit(t, get()); }
const Def* Merge  ::rebuild_(World& w, const Def* t, Defs o) const { return w.merge(t, o); }
const Def* Pack   ::rebuild_(World& w, const Def* t, Defs o) const { return w.pack(t->arity(), o[0]); }
const Def* Pi     ::rebuild_(World& w, const Def*  , Defs o) const { return w.pi(o[0], o[1], is_implicit()); }
const Def* Proxy  ::rebuild_(World& w, const Def* t, Defs o) const { return w.proxy(t, o, pass(), tag()); }
const Def* Sigma  ::rebuild_(World& w, const Def*  , Defs o) const { return w.sigma(o); }
const Def* Split  ::rebuild_(World& w, const Def* t, Defs o) const { return w.split(t, o[0]); }
const Def* Match  ::rebuild_(World& w, const Def*  , Defs o) const { return w.match(o); }
const Def* Tuple  ::rebuild_(World& w, const Def* t, Defs o) const { return w.tuple(t, o); }
const Def* Type   ::rebuild_(World& w, const Def*  , Defs o) const { return w.type(o[0]); }
const Def* UInc   ::rebuild_(World& w, const Def*  , Defs o) const { return w.uinc(o[0], offset()); }
const Def* UMax   ::rebuild_(World& w, const Def*  , Defs o) const { return w.umax(o); }
const Def* Uniq   ::rebuild_(World& w, const Def*  , Defs o) const { return w.uniq(o[0]); }
const Def* Var    ::rebuild_(World& w, const Def* t, Defs o) const { return w.var(t, o[0]->as_mut()); }

const Def* Axm    ::rebuild_(World& w, const Def* t, Defs ) const {
    if (&w != &world()) return w.axm(normalizer(), curry(), trip(), t, plugin(), tag(), sub())->set(dbg());
    assert(Checker::alpha<Checker::Check>(t, type()));
    return this;
}

template<bool up> const Def* TExt  <up>::rebuild_(World& w, const Def* t, Defs  ) const { return w.ext  <up>(t)->set(dbg()); }
template<bool up> const Def* TBound<up>::rebuild_(World& w, const Def*  , Defs o) const { return w.bound<up>(o)->set(dbg()); }

/*
 * stub
 */

Arr*    Arr   ::stub_(World& w, const Def* t) { return w.mut_arr  (t); }
Global* Global::stub_(World& w, const Def* t) { return w.global   (t, is_mutable()); }
Hole*   Hole  ::stub_(World& w, const Def* t) { return w.mut_hole (t); }
Lam*    Lam   ::stub_(World& w, const Def* t) { return w.mut_lam  (t->as<Pi>()); }
Pack*   Pack  ::stub_(World& w, const Def* t) { return w.mut_pack (t); }
Pi*     Pi    ::stub_(World& w, const Def* t) { return w.mut_pi   (t, is_implicit()); }
Sigma*  Sigma ::stub_(World& w, const Def* t) { return w.mut_sigma(t, num_ops()); }

/*
 * instantiate templates
 */

#ifndef DOXYGEN
template const Def* TExt<false>  ::rebuild_(World&, const Def*, Defs) const;
template const Def* TExt<true >  ::rebuild_(World&, const Def*, Defs) const;
template const Def* TBound<false>::rebuild_(World&, const Def*, Defs) const;
template const Def* TBound<true >::rebuild_(World&, const Def*, Defs) const;
#endif

// clang-format on

/*
 * immutabilize
 */

bool Def::is_immutabilizable() {
    if (!is_set()) return false;

    if (auto v = has_var()) {
        for (auto op : deps())
            if (op->free_vars().contains(v)) return false;
    }
    for (auto op : deps()) {
        for (auto mut : op->local_muts())
            if (mut == this) return false; // recursion
    }
    return true;
}

const Pi* Pi::immutabilize() {
    if (is_immutabilizable()) return world().pi(dom(), codom());
    return nullptr;
}

const Def* Sigma::immutabilize() {
    if (is_immutabilizable()) return static_cast<const Sigma*>(world().sigma(ops()));
    return nullptr;
}

const Def* Arr::immutabilize() {
    auto& w = world();
    if (is_immutabilizable()) return w.arr(shape(), body());

    if (auto n = Lit::isa(shape()); n && *n < w.flags().scalarize_threshold)
        return w.sigma(DefVec(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));

    return nullptr;
}

const Def* Pack::immutabilize() {
    auto& w = world();
    if (is_immutabilizable()) return w.pack(shape(), body());

    if (auto n = Lit::isa(shape()); n && *n < w.flags().scalarize_threshold)
        return w.tuple(DefVec(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));

    return nullptr;
}

/*
 * reduce
 */

Defs Def::reduce_(const Def* arg) const {
    if (auto var = has_var()) return world().reduce(var, arg);
    return {ops().begin() + reduction_offset(), num_ops() - reduction_offset()};
}

const Def* Def::refine(size_t i, const Def* new_op) const {
    auto new_ops = absl::FixedArray<const Def*>(num_ops());
    for (size_t j = 0, e = num_ops(); j != e; ++j) new_ops[j] = i == j ? new_op : op(j);
    return rebuild(type(), new_ops);
}

/*
 * Def - set
 */

Def* Def::set(Defs ops) {
    assert(ops.size() == num_ops());
    for (size_t i = 0, e = num_ops(); i != e; ++i) set(i, ops[i]);
    return this;
}

Def* Def::set(size_t i, const Def* def) {
#ifdef MIM_ENABLE_CHECKS
    if (world().watchpoints().contains(gid())) fe::breakpoint();
#endif
    invalidate();
    def = check(i, def);
    assert(def && !op(i) && curr_op_++ == i);
    ops_ptr()[i] = def;

    if (i + 1 == num_ops()) { // set last op, so check kind
        if (auto t = check()->zonk(); t != type()) type_ = t;
    }

    return this;
}

Def* Def::set_type(const Def* type) {
    invalidate();
    assert(curr_op_ == 0);
    type_ = type;
    return this;
}

Def* Def::unset() {
    invalidate();
#ifndef NDEBUG
    curr_op_ = 0;
#endif
    std::ranges::fill(ops_ptr(), ops_ptr() + num_ops(), nullptr);
    return this;
}

bool Def::is_set() const {
    if (num_ops() == 0) return true;
    bool result = ops().back();
    assert((!result || std::ranges::all_of(ops().rsubspan(1), [](auto op) { return op; }))
           && "the last operand is set but others in front of it aren't");
    return result;
}

/*
 * free_vars
 */

// clang-format off
const Def* Def::var() {
    if (var_) return var_;
    auto& w = world();

    if (w.is_frozen()) return nullptr;
    if (auto lam  = isa<Lam  >()) return w.var(lam ->dom(), lam);
    if (auto pi   = isa<Pi   >()) return w.var(pi  ->dom(),  pi);
    if (auto sig  = isa<Sigma>()) return w.var(sig,         sig);
    if (auto arr  = isa<Arr  >()) return w.var(w.type_idx(arr ->shape()), arr ); // TODO shapes like (2, 3)
    if (auto pack = isa<Pack >()) return w.var(w.type_idx(pack->shape()), pack); // TODO shapes like (2, 3)
    if (isa<Bound >()) return w.var(this, this);
    if (isa<Hole  >()) return nullptr;
    if (isa<Global>()) return nullptr;
    fe::unreachable();
}

Muts Def::local_muts() const {
    if (auto mut = isa_mut()) return Muts(mut);
    return muts_;
}

Vars Def::free_vars() const {
    if (auto mut = isa_mut()) return mut->free_vars();

    auto& vars = world().vars();
    auto fvs   = local_vars();
    for (auto mut : local_muts()) fvs = vars.merge(fvs, mut->free_vars());

    return fvs;
}

Vars Def::local_vars() const { return mut_ ? Vars() : vars_; }

Vars Def::free_vars() {
    if (mark_ == 0) {
        // fixed-point iteration to recompute free vars:
        // (run - 1) identifies the previous iteration; so make sure to offset run by 2 for the first iteration
        auto& w     = world();
        bool cyclic = false;
        w.next_run();
        free_vars<true>(cyclic, w.next_run());

        for (bool todo = cyclic; todo;) {
            todo = false;
            free_vars<false>(todo, w.next_run());
        }
    }

    return vars_;
}

template<bool init> Vars Def::free_vars(bool& todo, uint32_t run) {
    // If init == true  -> todo flag detects cycle
    // If init == false -> todo flag keeps track whether sth changed

    assert(isa_mut());
    // Recursively recompute free vars. If
    // * mark_ == 0:        Invalid - need to recompute.
    // * mark_ == run - 1:  Previous iteration - need to recompute.
    // * mark_ == run:      We are running in cycles within the *current* iteration of our fixed-point loop.
    // * otherwise:         Valid!
    if (mark_ != 0 && mark_ != run - 1) {
        if constexpr (init) todo |= mark_ == run;
        return vars_;
    }

    mark_ = run;

    auto fvs0  = vars_;
    auto fvs   = fvs0;
    auto& w    = world();
    auto& muts = w.muts();
    auto& vars = w.vars();

    for (auto op : deps()) {
        if constexpr (init) fvs = vars.merge(fvs, op->local_vars());

        for (auto mut : op->local_muts()) {
            if constexpr (init) mut->muts_ = muts.insert(mut->muts_, this); // register "this" as user of local_mut
            fvs = vars.merge(fvs, mut->free_vars<init>(todo, run));
        }
    }

    if (auto var = has_var()) fvs = vars.erase(fvs, var); // FV(λx.e) = FV(e) \ {x}

    if constexpr (!init) todo |= fvs0 != fvs;

    return vars_ = fvs;
}

void Def::invalidate() {
    if (mark_ != 0) {
        mark_ = 0;
        // TODO optimize if vars empty?
        for (auto mut : users()) mut->invalidate();
        vars_ = Vars();
        muts_ = Muts();
    }
}

bool Def::is_closed() const {
    if (local_vars().empty() && local_muts().empty()) return true;
#ifdef MIM_ENABLE_CHECKS
    assert(!is_external() || free_vars().empty());
#endif
    return free_vars().empty();
}

bool Def::is_open() const {
    if (!local_vars().empty()) return true;
    return !free_vars().empty();
}

/*
 * Def - misc
 */

Sym Def::sym(const char* s) const { return world().sym(s); }
Sym Def::sym(std::string_view s) const { return world().sym(s); }
Sym Def::sym(std::string s) const { return world().sym(std::move(s)); }

World& Def::world() const noexcept {
    for (auto def = this;; def = def->type()) {
        if (def->isa<Univ>()) return *def->world_;
        if (auto type = def->isa<Type>()) return *type->level()->type()->as<Univ>()->world_;
    }
}

const Def* Def::unfold_type() const {
    if (!type_) {
        auto& w = world();
        if (auto t = isa<Type>()) return w.type(w.uinc(t->level()));
        assert(isa<Univ>());
        return nullptr;
    }

    return type_;
}

std::string_view Def::node_name() const {
    switch (node()) {
#define CODE(name, _) \
    case Node::name: return #name;
        MIM_NODE(CODE)
#undef CODE
        default: fe::unreachable();
    }
}

Defs Def::deps() const noexcept {
    if (isa<Type>() || isa<Univ>()) return Defs();
    assert(type());
    return Defs(ops_ptr() - 1, (is_set() ? num_ops_ : 0) + 1);
}

u32 Def::judge() const noexcept {
    switch (node()) {
#define CODE(n, j) \
    case Node::n: return u32(j);
        MIM_NODE(CODE)
#undef CODE
        default: fe::unreachable();
    }
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

#ifndef NDEBUG
const Def* Def::debug_prefix(std::string prefix) const { return dbg_.set(world().sym(prefix + sym().str())), this; }
const Def* Def::debug_suffix(std::string suffix) const { return dbg_.set(world().sym(sym().str() + suffix)), this; }
#endif

/*
 * cmp
 */

Def::Cmp Def::cmp(const Def* a, const Def* b) {
    if (a == b) return Cmp::E;

    if (a->isa_imm() && b->isa_mut()) return Cmp::L;
    if (a->isa_mut() && b->isa_imm()) return Cmp::G;

    // clang-format off
    if (a->node()    != b->node()   ) return a->node()    < b->node()    ? Cmp::L : Cmp::G;
    if (a->num_ops() != b->num_ops()) return a->num_ops() < b->num_ops() ? Cmp::L : Cmp::G;
    if (a->flags()   != b->flags()  ) return a->flags()   < b->flags()   ? Cmp::L : Cmp::G;
    // clang-format on

    if (a->isa_mut() && b->isa_mut()) return Cmp::U;
    assert(a->isa_imm() && b->isa_imm());

    if (auto va = a->isa<Var>()) {
        auto vb = b->as<Var>();
        auto ma = va->mut();
        auto mb = vb->mut();
        if (ma->is_set() && ma->free_vars().contains(vb)) return Cmp::L;
        if (mb->is_set() && mb->free_vars().contains(va)) return Cmp::G;
        return Cmp::U;
    }

    // heuristic: iterate backwards as index (often a Lit) comes last and will faster find a solution
    for (size_t i = a->num_ops(); i-- != 0;)
        if (auto res = cmp(a->op(i), b->op(i)); res == Cmp::L || res == Cmp::G) return res;

    return cmp(a->type(), b->type());
}

template<Def::Cmp c> bool Def::cmp_(const Def* a, const Def* b) {
    auto res = cmp(a, b);
    if (res == Cmp::U) {
        a->world().WLOG("resorting to unstable gid-based compare for commute check");
        return c == Cmp::L ? a->gid() < b->gid() : a->gid() > b->gid();
    }
    return res == c;
}

// clang-format off
bool Def::less   (const Def* a, const Def* b) { return cmp_<Cmp::L>(a, b); }
bool Def::greater(const Def* a, const Def* b) { return cmp_<Cmp::G>(a, b); }
// clang-format on

// clang-format on

const Def* Def::arity() const {
    if (auto sigma = isa<Sigma>()) {
        auto n = sigma->num_ops();
        if (n != 1 || sigma->isa_mut()) return world().lit_nat(n);
        return sigma->op(0)->arity();
    }

    if (auto arr = isa<Arr>()) return arr->shape();
    if (auto t = type()) return t->arity();

    return world().lit_nat_1();
}

std::optional<nat_t> Def::isa_lit_arity() const {
    if (auto sigma = isa<Sigma>()) {
        auto n = sigma->num_ops();
        if (n != 1 || sigma->isa_mut()) return n;
        return sigma->op(0)->isa_lit_arity();
    }

    if (auto arr = isa<Arr>()) return Lit::isa(arr->shape());
    if (auto t = type()) return t->isa_lit_arity();

    return 1;
}

bool Def::equal(const Def* other) const {
    if (isa<Univ>() || this->isa_mut() || other->isa_mut()) return this == other;

    bool result = this->node() == other->node() && this->flags() == other->flags()
               && this->num_ops() == other->num_ops() && this->type() == other->type();

    for (size_t i = 0, e = num_ops(); result && i != e; ++i) result &= this->op(i) == other->op(i);

    return result;
}

void Def::make_external() { return world().make_external(this); }
void Def::make_internal() { return world().make_internal(this); }

std::string Def::unique_name() const { return sym().str() + "_"s + std::to_string(gid()); }

nat_t Def::num_tprojs() const {
    if (auto a = isa_lit_arity(); a && *a < world().flags().scalarize_threshold) return *a;
    return 1;
}

const Def* Def::proj(nat_t a, nat_t i) const {
    World& w = world();

    if (a == 1) {
        if (!type()) return this;
        if (!isa_mut<Sigma>() && !type()->isa_mut<Sigma>()) return this;
    }

    if (auto seq = isa<Seq>()) {
        if (seq->has_var()) return seq->reduce(world().lit_idx(a, i));
        return seq->body();
    }

    if (isa<Prod>()) return op(i);
    if (w.is_frozen()) return nullptr;

    return w.extract(this, a, i);
}

/*
 * Idx
 */

const Def* Idx::isa(const Def* def) {
    if (auto app = def->isa<App>()) {
        if (app->callee()->isa<Idx>()) return app->arg();
    }

    return nullptr;
}

std::optional<nat_t> Idx::isa_lit(const Def* def) {
    if (auto size = Idx::isa(def))
        if (auto l = Lit::isa(size)) return l;
    return {};
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

} // namespace mim
