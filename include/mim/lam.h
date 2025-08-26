#pragma once

#include <variant>

#include "mim/def.h"

namespace mim {

/// A [dependent function type](https://en.wikipedia.org/wiki/Dependent_type#%CE%A0_type).
/// @see Lam
class Pi : public Def, public Setters<Pi> {
protected:
    /// Constructor for an *immutable* Pi.
    Pi(const Def* type, const Def* dom, const Def* codom, bool implicit)
        : Def(Node, type, {dom, codom}, (flags_t)implicit) {}
    /// Constructor for a *mutable* Pi.
    Pi(const Def* type, bool implicit)
        : Def(Node, type, 2, implicit ? 1 : 0) {}

public:
    /// @name Get/Set implicit
    ///@{
    bool is_implicit() const { return flags(); }
    Pi* make_implicit() { return flags_ = (flags_t) true, this; }
    Pi* make_explicit() { return flags_ = (flags_t) false, this; }
    ///@}

    /// @name dom & codom
    /// @anchor pi_dom
    /// @see @ref proj
    ///@{
    const Def* dom() const { return op(0); }
    const Def* codom() const { return op(1); }
    MIM_PROJ(dom, const)
    MIM_PROJ(codom, const)
    ///@}

    /// @name Continuations
    /// @anchor continuations
    /// Checks certain properties of @p d regarding continuations.
    ///@{
    // clang-format off
    /// Is this a continuation - i.e. is the Pi::codom mim::Bot%tom?
    static const Pi* isa_cn(const Def* d) { return d->isa<Pi>() && d->as<Pi>()->codom()->node() == Node::Bot ? d->as<Pi>() : nullptr; }
    /// Is this a continuation (Pi::isa_cn) which has a Pi::ret_pi?
    static const Pi* isa_returning(const Def* d)  { return isa_cn(d) &&  d->as<Pi>()->ret_pi() ? d->as<Pi>() : nullptr; }
    /// Is this a continuation (Pi::isa_cn) that is **not** Pi::isa_returning?
    static const Pi* isa_basicblock(const Def* d) { return isa_cn(d) && !d->as<Pi>()->ret_pi() ? d->as<Pi>() : nullptr; }
    // clang-format on
    /// Is @p d an Pi::is_implicit (mutable) Pi?
    static Pi* isa_implicit(const Def* d) {
        if (auto pi = d->isa_mut<Pi>(); pi && pi->is_implicit()) return pi;
        return nullptr;
    }
    /// Yields the Pi::ret_pi() of @p d, if it is in fact a Pi.
    static const Pi* has_ret_pi(const Def* d) { return d->isa<Pi>() ? d->as<Pi>()->ret_pi() : nullptr; }
    ///@}

    /// @name Return Continuation
    /// @anchor return_continuation
    ///@{

    /// Yields the last Pi::dom, if Pi::isa_basicblock.
    const Pi* ret_pi() const;
    /// Pi::dom%ain of Pi::ret_pi.
    const Def* ret_dom() const { return ret_pi()->dom(); }
    ///@}

    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Pi>::set;
    Pi* set(const Def* dom, const Def* codom) { return set_dom(dom)->set_codom(codom); }
    Pi* set_dom(const Def* dom) { return Def::set(0, dom)->as<Pi>(); }
    Pi* set_dom(Defs doms);
    Pi* set_codom(const Def* codom) { return Def::set(1, codom)->as<Pi>(); }
    Pi* unset() { return Def::unset()->as<Pi>(); }
    ///@}

    /// @name Type Checking
    ///@{
    const Def* check(size_t, const Def*) final;
    const Def* check() final;
    static const Def* infer(const Def* dom, const Def* codom);
    ///@}

    /// @name Rebuild
    ///@{
    Pi* stub(const Def* type) { return stub_(world(), type)->set(dbg()); }
    const Pi* immutabilize() final;
    const Def* reduce(const Def* arg) const { return Def::reduce(arg).front(); }
    constexpr size_t reduction_offset() const noexcept final { return 1; }
    ///@}

    static constexpr auto Node      = mim::Node::Pi;
    static constexpr size_t Num_Ops = 2;

private:
    const Def* rebuild_(World&, const Def*, Defs) const final;
    Pi* stub_(World&, const Def*) final;

    friend class World;
};

/// A function.
/// @see Pi
class Lam : public Def, public Setters<Lam> {
private:
    Lam(const Pi* pi, const Def* filter, const Def* body)
        : Def(Node, pi, {filter, body}, 0) {}
    Lam(const Pi* pi)
        : Def(Node, pi, 2, 0) {}

public:
    using Filter = std::variant<bool, const Def*>;

    /// @name ops
    ///@{
    const Def* filter() const { return op(0); }
    const Def* body() const { return op(1); }
    ///@}

    /// @name type
    /// @anchor lam_dom
    /// @see @ref proj
    ///@{
    const Pi* type() const { return Def::type()->as<Pi>(); }
    const Def* dom() const { return type()->dom(); }
    const Def* codom() const { return type()->codom(); }
    MIM_PROJ(dom, const)
    MIM_PROJ(codom, const)
    ///@}

    /// @name Continuations
    /// @see @ref continuations "Pi: Continuations"
    ///@{
    // clang-format off
    static const Lam* isa_cn(const Def* d) { return Pi::isa_cn(d->type()) ? d->isa<Lam>() : nullptr; }
    static const Lam* isa_basicblock(const Def* d) { return Pi::isa_basicblock(d->type()) ? d->isa<Lam>() : nullptr; }
    static const Lam* isa_returning(const Def* d)  { return Pi::isa_returning (d->type()) ? d->isa<Lam>() : nullptr; }
    static Lam* isa_mut_cn(const Def* d) { return isa_cn(d) ? d->isa_mut<Lam>() : nullptr; }                ///< Only for mutables.
    static Lam* isa_mut_basicblock(const Def* d) { return isa_basicblock(d) ? d->isa_mut<Lam>(): nullptr; } ///< Only for mutables.
    static Lam* isa_mut_returning(const Def* d)  { return isa_returning (d) ? d->isa_mut<Lam>(): nullptr; } ///< Only for mutables.
    // clang-format on
    ///@}

    /// @name Return Continuation
    /// @see @ref return_continuation "Pi: Return Continuation"
    ///@{
    const Pi* ret_pi() const { return type()->ret_pi(); }
    const Def* ret_dom() const { return ret_pi()->dom(); }
    /// Yields the Lam::var of the Lam::ret_pi.
    const Def* ret_var() { return type()->ret_pi() ? var(num_vars() - 1) : nullptr; }
    ///@}

    /// @name Setters
    /// Lam::Filter is a `std::variant<bool, const Def*>` that lets you set the Lam::filter() like this:
    /// ```cpp
    /// lam1->app(true, f, arg);
    /// lam2->app(my_filter_def, f, arg);
    /// ```
    /// @note The filter belongs to the *Lam%bda* and **not** the body.
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Lam>::set;
    Lam* set(Filter filter, const Def* body) { return set_filter(filter)->set_body(body); }
    Lam* set_filter(Filter);                                                ///< Set filter first.
    Lam* set_body(const Def* body) { return Def::set(1, body)->as<Lam>(); } ///< Set body second.
    /// Set body to an App of @p callee and @p arg.
    Lam* app(Filter filter, const Def* callee, const Def* arg);
    /// Set body to an App of @p callee and @p args.
    Lam* app(Filter filter, const Def* callee, Defs args);
    /// Set body to an App of `(f, t)#cond mem` or `(f, t)#cond ()` if @p mem is `nullptr`.
    Lam* branch(Filter filter, const Def* cond, const Def* t, const Def* f, const Def* mem = nullptr);
    Lam* set(Defs ops) { return Def::set(ops)->as<Lam>(); }
    Lam* unset() { return Def::unset()->as<Lam>(); }
    ///@}

    /// @name Rebuild
    ///@{
    Lam* stub(const Def* type) { return stub_(world(), type)->set(dbg()); }
    const Def* reduce_body(const Def* arg) const { return reduce(arg).back(); }
    constexpr size_t reduction_offset() const noexcept final { return 0; }
    ///@}

    /// @name Eta-Conversion
    ///@{
    static const Def* eta_expand(Filter, const Def* f);
    /// Yields body(), if eta-convertible and `nullptr` otherwise.
    /// η-convertible means: `lm x = body x` where `x` ∉ `body`.
    const Def* eta_reduce() const;
    ///@}

    /// @name Type Checking
    ///@{
    const Def* check(size_t, const Def*) final;
    ///@}

    static constexpr auto Node      = mim::Node::Lam;
    static constexpr size_t Num_Ops = 2;

private:
    const Def* rebuild_(World&, const Def*, Defs) const final;
    Lam* stub_(World&, const Def*) final;

    friend class World;
};

/// @name Lam
/// GIDSet / GIDMap keyed by Lam::gid of `Lam*`.
///@{
template<class To>
using LamMap  = GIDMap<Lam*, To>;
using LamSet  = GIDSet<Lam*>;
using Lam2Lam = LamMap<Lam*>;
///@}

class App : public Def, public Setters<App> {
private:
    App(const Axm* axm, u8 curry, u8 trip, const Def* type, const Def* callee, const Def* arg)
        : Def(Node, type, {callee, arg}, 0) {
        axm_   = axm;
        curry_ = curry;
        trip_  = trip;
    }

public:
    using Setters<App>::set;

    /// @name callee
    ///@{
    const Def* callee() const { return op(0); }
    const App* decurry() const { return callee()->as<App>(); } ///< Returns App::callee again as App.
    const Pi* callee_type() const { return callee()->type()->as<Pi>(); }
    ///@}

    /// @name arg
    /// @anchor app_arg
    /// @see @ref proj
    ///@{
    const Def* arg() const { return op(1); }
    MIM_PROJ(arg, const)
    ///@}

    /// @name Get axm, current curry counter and trip count
    ///@{
    const Axm* axm() const { return axm_; }
    u8 curry() const { return curry_; }
    u8 trip() const { return trip_; }
    ///@}

    static constexpr auto Node      = mim::Node::App;
    static constexpr size_t Num_Ops = 2;

private:
    const Def* rebuild_(World&, const Def*, Defs) const final;

    friend class World;
};

/// @name Helpers to work with Functions
///@{
inline const App* isa_callee(const Def* def, size_t i) { return i == 0 ? def->isa<App>() : nullptr; }

/// These are Lam%s that are neither `nullptr`, nor Lam::is_external, nor Lam::is_unset.
inline Lam* isa_workable(Lam* lam) {
    if (!lam || lam->is_external() || !lam->is_set()) return nullptr;
    return lam;
}

inline std::pair<const App*, Lam*> isa_apped_mut_lam(const Def* def) {
    if (auto app = def->isa<App>()) return {app, app->callee()->isa_mut<Lam>()};
    return {nullptr, nullptr};
}

/// Yields curried App%s in a flat `std::deque<const App*>`.
std::deque<const App*> decurry(const Def*);

/// The high level view is:
/// ```
/// f: B -> C
/// g: A -> B
/// f o g := λ x. f(g(x)) : A -> C
/// ```
/// In CPS the types look like:
/// ```
/// f:  Cn[B, Cn C]
/// g:  Cn[A, Cn B]
/// h = f o g
/// h:  Cn[A, cn C]
/// h = λ (a ret_h) = g (a, h')
/// h': Cn B
/// h'= λ b = f (b, ret_h)
/// ```
const Def* compose_cn(const Def* f, const Def* g);

/// Helper function to cope with the fact that normalizers take all arguments and not only its axm arguments.
std::pair<const Def*, DefVec> collect_args(const Def* def);
///@}

} // namespace mim
