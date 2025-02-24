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
    /// Constructor for a *mut*able Pi.
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
    Ref dom() const { return op(0); }
    Ref codom() const { return op(1); }
    MIM_PROJ(dom, const)
    MIM_PROJ(codom, const)
    ///@}

    /// @name Continuations
    /// @anchor continuations
    /// Checks certain properties of @p d regarding continuations.
    ///@{
    // clang-format off
    /// Is this a continuation - i.e. is the Pi::codom mim::Bot%tom?
    static const Pi* isa_cn(Ref d) { return d->isa<Pi>() && d->as<Pi>()->codom()->node() == Node::Bot ? d->as<Pi>() : nullptr; }
    /// Is this a continuation (Pi::isa_cn) which has a Pi::ret_pi?
    static const Pi* isa_returning(Ref d)  { return isa_cn(d) &&  d->as<Pi>()->ret_pi() ? d->as<Pi>() : nullptr; }
    /// Is this a continuation (Pi::isa_cn) that is **not** Pi::isa_returning?
    static const Pi* isa_basicblock(Ref d) { return isa_cn(d) && !d->as<Pi>()->ret_pi() ? d->as<Pi>() : nullptr; }
    // clang-format on
    /// Is @p d an Pi::is_implicit (mutable) Pi?
    static Pi* isa_implicit(Ref d) {
        if (auto pi = d->isa_mut<Pi>(); pi && pi->is_implicit()) return pi;
        return nullptr;
    }
    /// Yields the Pi::ret_pi() of @p d, if it is in fact a Pi.
    static const Pi* has_ret_pi(Ref d) { return d->isa<Pi>() ? d->as<Pi>()->ret_pi() : nullptr; }
    ///@}

    /// @name Return Continuation
    /// @anchor return_continuation
    ///@{

    /// Yields the last Pi::dom, if Pi::isa_basicblock.
    const Pi* ret_pi() const;
    /// Pi::dom%ain of Pi::ret_pi.
    Ref ret_dom() const { return ret_pi()->dom(); }
    ///@}

    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Pi>::set;
    Pi* set(Ref dom, Ref codom) { return set_dom(dom)->set_codom(codom); }
    Pi* set_dom(Ref dom) { return Def::set(0, dom)->as<Pi>(); }
    Pi* set_dom(Defs doms);
    Pi* set_codom(Ref codom) { return Def::set(1, codom)->as<Pi>(); }
    Pi* unset() { return Def::unset()->as<Pi>(); }
    ///@}

    /// @name Type Checking
    ///@{
    Ref check(size_t, Ref) override;
    Ref check() override;
    static Ref infer(Ref dom, Ref codom);
    ///@}

    /// @name Rebuild
    ///@{
    Ref reduce(Ref arg) const { return Def::reduce(1, arg); }
    Pi* stub(Ref type) { return stub_(world(), type)->set(dbg()); }
    const Pi* immutabilize() override;
    ///@}

    static constexpr auto Node = Node::Pi;

private:
    Ref rebuild_(World&, Ref, Defs) const override;
    Pi* stub_(World&, Ref) override;

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
    using Filter = std::variant<bool, Ref>;

    /// @name ops
    ///@{
    Ref filter() const { return op(0); }
    Ref body() const { return op(1); }
    ///@}

    /// @name type
    /// @anchor lam_dom
    /// @see @ref proj
    ///@{
    const Pi* type() const { return Def::type()->as<Pi>(); }
    Ref dom() const { return type()->dom(); }
    Ref codom() const { return type()->codom(); }
    MIM_PROJ(dom, const)
    MIM_PROJ(codom, const)
    ///@}

    /// @name Continuations
    /// @see @ref continuations "Pi: Continuations"
    ///@{
    // clang-format off
    static const Lam* isa_cn(Ref d) { return Pi::isa_cn(d->type()) ? d->isa<Lam>() : nullptr; }
    static const Lam* isa_basicblock(Ref d) { return Pi::isa_basicblock(d->type()) ? d->isa<Lam>() : nullptr; }
    static const Lam* isa_returning(Ref d)  { return Pi::isa_returning (d->type()) ? d->isa<Lam>() : nullptr; }
    static Lam* isa_mut_cn(Ref d) { return isa_cn(d) ? d->isa_mut<Lam>() : nullptr; }                ///< Only for mutables.
    static Lam* isa_mut_basicblock(Ref d) { return isa_basicblock(d) ? d->isa_mut<Lam>(): nullptr; } ///< Only for mutables.
    static Lam* isa_mut_returning(Ref d)  { return isa_returning (d) ? d->isa_mut<Lam>(): nullptr; } ///< Only for mutables.
    // clang-format on
    ///@}

    /// @name Return Continuation
    /// @see @ref return_continuation "Pi: Return Continuation"
    ///@{
    const Pi* ret_pi() const { return type()->ret_pi(); }
    Ref ret_dom() const { return ret_pi()->dom(); }
    /// Yields the Lam::var of the Lam::ret_pi.
    Ref ret_var() { return type()->ret_pi() ? var(num_vars() - 1) : nullptr; }
    ///@}

    /// @name Setters
    /// Lam::Filter is a `std::variant<bool, const Def*>` that lets you set the Lam::filter() like this:
    /// ```cpp
    /// lam1->app(true, f, arg);
    /// lam2->app(my_filter_def, f, arg);
    /// ```
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Lam>::set;
    Lam* set(Filter filter, Ref body) { return set_filter(filter)->set_body(body); }
    Lam* set_filter(Filter);                                         ///< Set filter first.
    Lam* set_body(Ref body) { return Def::set(1, body)->as<Lam>(); } ///< Set body second.
    /// Set body to an App of @p callee and @p arg.
    Lam* app(Filter filter, Ref callee, Ref arg);
    /// Set body to an App of @p callee and @p args.
    Lam* app(Filter filter, Ref callee, Defs args);
    /// Set body to an App of `(f, t)#cond mem` or `(f, t)#cond ()` if @p mem is `nullptr`.
    Lam* branch(Filter filter, Ref cond, Ref t, Ref f, Ref mem = nullptr);
    Lam* test(Filter filter, Ref val, Ref idx, Ref match, Ref clash, Ref mem);
    Lam* set(Defs ops) { return Def::set(ops)->as<Lam>(); }
    Lam* unset() { return Def::unset()->as<Lam>(); }
    ///@}

    Lam* stub(Ref type) { return stub_(world(), type)->set(dbg()); }

    /// @name Type Checking
    ///@{
    Ref check(size_t, Ref) override;
    ///@}

    static constexpr auto Node = Node::Lam;

private:
    Ref rebuild_(World&, Ref, Defs) const override;
    Lam* stub_(World&, Ref) override;

    friend class World;
};

/// @name Lam
/// GIDSet / GIDMap keyed by Lam::gid of `Lam*`.
///@{
template<class To> using LamMap = GIDMap<Lam*, To>;
using LamSet                    = GIDSet<Lam*>;
using Lam2Lam                   = LamMap<Lam*>;
///@}

class App : public Def, public Setters<App> {
private:
    App(const Axiom* axiom, u8 curry, u8 trip, const Def* type, const Def* callee, const Def* arg)
        : Def(Node, type, {callee, arg}, 0) {
        axiom_ = axiom;
        curry_ = curry;
        trip_  = trip;
    }

public:
    using Setters<App>::set;

    /// @name callee
    ///@{
    Ref callee() const { return op(0); }
    const App* decurry() const { return callee()->as<App>(); } ///< Returns App::callee again as App.
    const Pi* callee_type() const { return callee()->type()->as<Pi>(); }
    ///@}

    /// @name arg
    /// @anchor app_arg
    /// @see @ref proj
    ///@{
    Ref arg() const { return op(1); }
    MIM_PROJ(arg, const)
    ///@}

    /// @name Get axiom, current curry counter and trip count
    ///@{
    const Axiom* axiom() const { return axiom_; }
    u8 curry() const { return curry_; }
    u8 trip() const { return trip_; }
    ///@}

    static constexpr auto Node = Node::App;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

/// @name Helpers to work with Functions
///@{
inline const App* isa_callee(Ref def, size_t i) { return i == 0 ? def->isa<App>() : nullptr; }

/// These are Lam%s that are neither `nullptr`, nor Lam::is_external, nor Lam::is_unset.
inline Lam* isa_workable(Lam* lam) {
    if (!lam || lam->is_external() || !lam->is_set()) return nullptr;
    return lam;
}

inline std::pair<const App*, Lam*> isa_apped_mut_lam(Ref def) {
    if (auto app = def->isa<App>()) return {app, app->callee()->isa_mut<Lam>()};
    return {nullptr, nullptr};
}

/// Yields curried App%s in a flat `std::deque<const App*>`.
std::deque<const App*> decurry(Ref);

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
Ref compose_cn(Ref f, Ref g);

/// Helper function to cope with the fact that normalizers take all arguments and not only its axiom arguments.
std::pair<Ref, DefVec> collect_args(Ref def);
///@}

} // namespace mim
