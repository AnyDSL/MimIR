#include "thorin/def.h"

#include <algorithm>

#include "thorin/rewrite.h"
#include "thorin/world.h"

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
    , valid_(false)
    , num_ops_(ops.size())
    , type_(type) {
    std::ranges::copy(ops, ops_ptr());
    gid_ = world().next_gid();

    if (auto var = isa<Var>()) {
        vars_.local = world().vars(var);
        muts_.local = Muts();
    } else {
        vars_.local = Vars();
        muts_.local = Muts();

        for (auto op : extended_ops()) {
            vars_.local = world().merge(vars_.local, op->local_vars());
            muts_.local = world().merge(muts_.local, op->local_muts());
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
    gid_               = world().next_gid();
    hash_              = murmur3(gid());
    var_               = nullptr;
    vars_.free         = Vars();
    muts_.fv_consumers = Muts();
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

#ifndef DOXYGEN // TODO Doxygen doesn't expand THORIN_DEF_MIXIN
Ref Infer    ::rebuild_(World&,   Ref,   Defs  ) const { fe::unreachable(); }
Ref Global   ::rebuild_(World&,   Ref,   Defs  ) const { fe::unreachable(); }
Ref Idx      ::rebuild_(World& w, Ref  , Defs  ) const { return w.type_idx(); }
Ref Nat      ::rebuild_(World& w, Ref  , Defs  ) const { return w.type_nat(); }
Ref Univ     ::rebuild_(World& w, Ref  , Defs  ) const { return w.univ(); }
Ref Ac       ::rebuild_(World& w, Ref t, Defs o) const { return w.ac(t, o); }
Ref App      ::rebuild_(World& w, Ref  , Defs o) const { return w.app(o[0], o[1]); }
Ref Arr      ::rebuild_(World& w, Ref  , Defs o) const { return w.arr(o[0], o[1]); }
Ref Extract  ::rebuild_(World& w, Ref  , Defs o) const { return w.extract(o[0], o[1]); }
Ref Insert   ::rebuild_(World& w, Ref  , Defs o) const { return w.insert(o[0], o[1], o[2]); }
Ref Lam      ::rebuild_(World& w, Ref t, Defs o) const { return w.lam(t->as<Pi>(), o[0], o[1]); }
Ref Lit      ::rebuild_(World& w, Ref t, Defs  ) const { return w.lit(t, get()); }
Ref Pack     ::rebuild_(World& w, Ref t, Defs o) const { return w.pack(t->arity(), o[0]); }
Ref Pi       ::rebuild_(World& w, Ref  , Defs o) const { return w.pi(o[0], o[1], is_implicit()); }
Ref Pick     ::rebuild_(World& w, Ref t, Defs o) const { return w.pick(t, o[0]); }
Ref Proxy    ::rebuild_(World& w, Ref t, Defs o) const { return w.proxy(t, o, pass(), tag()); }
Ref Sigma    ::rebuild_(World& w, Ref  , Defs o) const { return w.sigma(o); }
Ref Singleton::rebuild_(World& w, Ref  , Defs o) const { return w.singleton(o[0]); }
Ref Type     ::rebuild_(World& w, Ref  , Defs o) const { return w.type(o[0]); }
Ref Test     ::rebuild_(World& w, Ref  , Defs o) const { return w.test(o[0], o[1], o[2], o[3]); }
Ref Tuple    ::rebuild_(World& w, Ref t, Defs o) const { return w.tuple(t, o); }
Ref UInc     ::rebuild_(World& w, Ref  , Defs o) const { return w.uinc(o[0], offset()); }
Ref UMax     ::rebuild_(World& w, Ref  , Defs o) const { return w.umax(o); }
Ref Var      ::rebuild_(World& w, Ref t, Defs o) const { return w.var(t, o[0]->as_mut()); }
Ref Vel      ::rebuild_(World& w, Ref t, Defs o) const { return w.vel(t, o[0])->set(dbg()); }

Ref Axiom    ::rebuild_(World& w, Ref t, Defs ) const {
    if (&w != &world()) return w.axiom(normalizer(), curry(), trip(), t, plugin(), tag(), sub())->set(dbg());
    assert(Check::alpha(t, type()));
    return this;
}

template<bool up> Ref TExt  <up>::rebuild_(World& w, Ref t, Defs  ) const { return w.ext  <up>(t)->set(dbg()); }
template<bool up> Ref TBound<up>::rebuild_(World& w, Ref  , Defs o) const { return w.bound<up>(o)->set(dbg()); }
#endif

/*
 * stub
 */

Arr*    Arr   ::stub_(World& w, Ref t) { return w.mut_arr  (t); }
Global* Global::stub_(World& w, Ref t) { return w.global   (t, is_mutable()); }
Infer*  Infer ::stub_(World& w, Ref t) { return w.mut_infer(t); }
Lam*    Lam   ::stub_(World& w, Ref t) { return w.mut_lam  (t->as<Pi>()); }
Pack*   Pack  ::stub_(World& w, Ref t) { return w.mut_pack (t); }
Pi*     Pi    ::stub_(World& w, Ref t) { return w.mut_pi   (t, is_implicit()); }
Sigma*  Sigma ::stub_(World& w, Ref t) { return w.mut_sigma(t, num_ops()); }

template<bool up> TBound<up>* TBound<up>::stub_(World& w, Ref t) { return w.mut_bound<up>(t, num_ops()); }
template<bool up> TExt  <up>* TExt  <up>::stub_(World&  , Ref  ) { fe::unreachable(); }

/*
 * instantiate templates
 */

#ifndef DOXYGEN
template Ref            TExt<false>  ::rebuild_(World&, Ref, Defs) const;
template Ref            TExt<true >  ::rebuild_(World&, Ref, Defs) const;
template Ref            TBound<false>::rebuild_(World&, Ref, Defs) const;
template Ref            TBound<true >::rebuild_(World&, Ref, Defs) const;
template TBound<false>* TBound<false>::stub_(World&, Ref);
template TBound<true >* TBound<true >::stub_(World&, Ref);
template TExt<false>*   TExt<false>  ::stub_(World&, Ref);
template TExt<true >*   TExt<true >  ::stub_(World&, Ref);
#endif

// clang-format on

/*
 * immutabilize
 */

// TODO check for recursion
const Pi* Pi::immutabilize() {
    if (auto var = has_var(); var && codom()->free_vars().contains(var)) return nullptr;
    return world().pi(dom(), codom());
}

const Def* Sigma::immutabilize() {
    if (auto v = has_var(); v && std::ranges::any_of(ops(), [v](auto op) { return op->free_vars().contains(v); }))
        return nullptr;
    return static_cast<const Sigma*>(*world().sigma(ops()));
}

const Def* Arr::immutabilize() {
    auto& w = world();
    if (auto var = has_var(); !var || !body()->free_vars().contains(var)) return w.arr(shape(), body());

    if (auto n = Lit::isa(shape()); n && *n < w.flags().scalerize_threshold)
        return w.sigma(DefVec(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));

    return nullptr;
}

const Def* Pack::immutabilize() {
    auto& w = world();
    if (auto var = has_var(); !var || !body()->free_vars().contains(var)) return w.pack(shape(), body());

    if (auto n = Lit::isa(shape()); n && *n < w.flags().scalerize_threshold)
        return w.tuple(DefVec(*n, [&](size_t i) { return reduce(w.lit_idx(*n, i)); }));

    return nullptr;
}

/*
 * reduce
 */

const Def* Arr::reduce(const Def* arg) const {
    if (auto mut = isa_mut<Arr>()) return rewrite(1, mut, arg);
    return body();
}

const Def* Pack::reduce(const Def* arg) const {
    if (auto mut = isa_mut<Pack>()) return rewrite(0, mut, arg);
    return body();
}

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
    return rebuild(type(), new_ops);
}

/*
 * Def - set
 */

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
    invalidate();
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
    invalidate();
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
    invalidate();
    assert(op(i) && op(i)->uses_.contains(Use(this, i)));
    op(i)->uses_.erase(Use(this, i));
    ops_ptr()[i] = nullptr;
    return this;
}

Def* Def::set_type(const Def* type) {
    if (type_ != type) {
        invalidate();
        if (type_ != nullptr) unset_type();
        type_ = type;
        type->uses_.emplace(this, Use::Type);
    }
    return this;
}

void Def::unset_type() {
    if (type_) {
        invalidate();
        if (!type_->dep_const()) {
            assert(type_->uses_.contains(Use(this, Use::Type)));
            type_->uses_.erase(Use(this, Use::Type));
        }
        type_ = nullptr;
    }
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

Muts Def::local_muts() const {
    if (auto mut = isa_mut()) return world().muts(mut);
    return muts_.local;
}

Vars Def::free_vars() const {
    if (auto mut = isa_mut()) return mut->free_vars();

    auto vars = local_vars();
    for (auto mut : local_muts()) vars = world().merge(vars, mut->free_vars());
    return vars;
}

Vars Def::free_vars() {
    if (!isa_mut()) return const_cast<const Def*>(this)->free_vars();
    if (!is_set()) return {};

    if (!valid_) {
        // fixed-point interation to recompute vars_.free
        uint32_t run = 1;
        for (bool todo = true; todo;) {
            todo = false;
            free_vars(todo, ++run);
        }
        validate();
    }

    return vars_.free;
}

Vars Def::free_vars(bool& todo, uint32_t run) {
    // Recursively recompute vars_.free. If
    // * valid_ == true: no recomputation is necessary
    // * mark_ == run: We are running in cycles within the *current* iteration of our fixed-point loop
    if (valid_ || mark_ == run) return vars_.free;
    mark_ = run;

    auto fvs0 = vars_.free;
    auto fvs  = fvs0;

    for (auto op : extended_ops()) fvs = world().merge(fvs, op->local_vars());

    for (auto op : extended_ops()) {
        for (auto local_mut : op->local_muts()) {
            local_mut->muts_.fv_consumers = world().insert(local_mut->muts_.fv_consumers, this);
            fvs                           = world().merge(fvs, local_mut->free_vars(todo, run));
        }
    }

    if (auto var = has_var()) fvs = world().erase(fvs, var); // FV(λx.e) = FV(e) \ {x}

    todo |= fvs0 != fvs;
    return vars_.free = fvs;
}

void Def::validate() {
    valid_ = true;
    mark_  = 0;

    for (auto op : extended_ops()) {
        for (auto local_mut : op->local_muts())
            if (!local_mut->valid_) local_mut->validate();
    }
}

void Def::invalidate() {
    if (valid_) {
        valid_ = false;
        for (auto mut : fv_consumers()) mut->invalidate();
        vars_.free.clear();
        muts_.fv_consumers.clear();
    }
}

bool Def::is_closed() const {
    if (local_vars().empty() && local_muts().empty()) return true;
#ifdef THORIN_ENABLE_CHECKS
    assert(!is_external() || free_vars().empty());
#endif
    return free_vars().empty();
}

bool Def::is_open() const {
    if (!local_vars().empty() || !local_muts().empty()) return true;
    return !free_vars().empty();
}
/*
 * Def - misc
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
const Def* Def::debug_prefix(std::string prefix) const { return dbg_.set(world().sym(prefix + sym().str())), this; }
const Def* Def::debug_suffix(std::string suffix) const { return dbg_.set(world().sym(sym().str() + suffix)), this; }
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
