#pragma once

#include <variant>

#include "thorin/def.h"

namespace thorin {

/// A function type AKA Pi type.
class Pi : public Def {
protected:
    /// Constructor for an *immutable* Pi.
    Pi(const Def* type, const Def* dom, const Def* codom, bool implicit)
        : Def(Node, type, {dom, codom}, implicit ? 1 : 0) {}
    /// Constructor for a *mut*able Pi.
    Pi(const Def* type, bool implicit)
        : Def(Node, type, 2, implicit ? 1 : 0) {}

public:
    bool implicit() const { return flags(); }

    /// @name dom
    ///@{
    Ref dom() const { return op(0); }
    THORIN_PROJ(dom, const)
    ///@}

    /// @name codom
    ///@{
    Ref codom() const { return op(1); }
    THORIN_PROJ(codom, const)
    ///@}

    /// @name Continuations
    /// @anchor continuations
    ///@{
    /// Checks certain properties of this Pi regarding continuations.
    // clang-format off
    /// Is this a continuation - i.e. the Pi::codom is Bot?
    static const Pi* isa_cn(Ref d) { return d->isa<Pi>() && d->as<Pi>()->codom()->node() == Node::Bot ? d->as<Pi>() : nullptr; }
    /// Is this a continuation (Pi::isa_cn) which has a Pi::ret_pi?
    static const Pi* isa_returning(Ref d)  { return isa_cn(d) &&  d->as<Pi>()->ret_pi() ? d->as<Pi>() : nullptr; }
    /// Is this a continuation (Pi::isa_cn) that is **not** Pi::isa_returning?
    static const Pi* isa_basicblock(Ref d) { return isa_cn(d) && !d->as<Pi>()->ret_pi() ? d->as<Pi>() : nullptr; }
    // clang-format on
    ///@}

    /// @name Return Continuation
    /// @anchor return_continuation
    ///@{
    const Pi* ret_pi() const;                       ///< Yields the last Pi::dom, if it is a Pi::isa_basicblock.
    Ref ret_dom() const { return ret_pi()->dom(); } ///< Pi::dom%ain of Pi::ret_pi.
    ///@}

    /// @name Setters
    ///@{
    Pi* set_dom(const Def* dom) { return Def::set(0, dom)->as<Pi>(); }
    Pi* set_dom(Defs doms);
    Pi* set_codom(const Def* codom) { return Def::set(1, codom)->as<Pi>(); }
    ///@}

    static const Def* infer(const Def* dom, const Def* codom);

    /// @name Virtual Methods
    ///@{
    size_t first_dependend_op() override { return 1; }
    const Pi* restructure() override;
    void check() override;
    ///@}

    THORIN_DEF_MIXIN(Pi)
    Pi* stub_(World&, Ref) override;
};

class Lam : public Def {
private:
    Lam(const Pi* pi, const Def* filter, const Def* body)
        : Def(Node, pi, {filter, body}, 0) {}
    Lam(const Pi* pi)
        : Def(Node, pi, 2, 0) {}

public:
    /// @name type
    ///@{
    const Pi* type() const { return Def::type()->as<Pi>(); }
    Ref dom() const { return type()->dom(); }
    THORIN_PROJ(dom, const)
    Ref codom() const { return type()->codom(); }
    THORIN_PROJ(codom, const)
    ///@}

    /// @name ops
    ///@{
    Ref filter() const { return op(0); }
    Ref body() const { return op(1); }
    ///@}

    /// @name Continuations
    ///@{
    /// @see @ref continuations
    // clang-format off
    static const Lam* isa_cn(Ref d) { return Pi::isa_cn(d->type()) ? d->isa<Lam>() : nullptr; }
    static const Lam* isa_basicblock(Ref d) { return Pi::isa_basicblock(d->type()) ? d->isa<Lam>() : nullptr; }
    static const Lam* isa_returning(Ref d)  { return Pi::isa_returning (d->type()) ? d->isa<Lam>() : nullptr; }
    static Lam* isa_mut_cn(Ref d) { return isa_cn(d) ? d->isa_mut<Lam>() : nullptr; }
    static Lam* isa_mut_basicblock(Ref d) { return isa_basicblock(d) ? d->isa_mut<Lam>(): nullptr; }
    static Lam* isa_mut_returning(Ref d)  { return isa_returning (d) ? d->isa_mut<Lam>(): nullptr; }
    // clang-format on
    ///@}

    /// @name Return Continuation
    ///@{
    /// @see @ref return_continuation
    const Pi* ret_pi() const { return type()->ret_pi(); }
    Ref ret_dom() const { return ret_pi()->dom(); }
    /// Yields the Lam::var of the Lam::ret_pi.
    Ref ret_var() { return type()->ret_pi() ? var(num_vars() - 1) : nullptr; }
    ///@}

    /// @name Setters
    ///@{
    /// Lam::Filter is a `std::variant<bool, const Def*>` that lets you set the Lam::filter() like this:
    /// ```cpp
    /// lam1->app(true, f, arg);
    /// lam2->app(my_filter_def, f, arg);
    /// ```
    using Filter = std::variant<bool, const Def*>;
    Lam* set(Defs ops) { return Def::set(ops)->as<Lam>(); }
    Lam* set(Filter filter, const Def* body) {
        set_filter(filter);
        return set_body(body);
    }
    Lam* set_filter(Filter);
    Lam* set_body(const Def* body) { return Def::set(1, body)->as<Lam>(); }
    /// Set body to an App of @p callee and @p arg.
    Lam* app(Filter filter, const Def* callee, const Def* arg);
    /// Set body to an App of @p callee and @p args.
    Lam* app(Filter filter, const Def* callee, Defs args);
    /// Set body to an App of `(f, t)#cond mem`.
    Lam* branch(Filter filter, const Def* cond, const Def* t, const Def* f, const Def* mem);
    Lam* test(Filter filter, const Def* val, const Def* idx, const Def* match, const Def* clash, const Def* mem);
    ///@}

    /// @name Virtual Methods
    ///@{
    void check() override;
    ///@}

    THORIN_DEF_MIXIN(Lam)
    Lam* stub_(World&, Ref) override;
};

template<class To>
using LamMap  = GIDMap<Lam*, To>;
using LamSet  = GIDSet<Lam*>;
using Lam2Lam = LamMap<Lam*>;

class App : public Def {
private:
    App(const Axiom* axiom, u8 curry, u8 trip, const Def* type, const Def* callee, const Def* arg)
        : Def(Node, type, {callee, arg}, 0) {
        axiom_ = axiom;
        curry_ = curry;
        trip_  = trip;
    }

public:
    /// @name ops
    ///@{
    const Def* callee() const { return op(0); }
    const App* decurry() const { return callee()->as<App>(); } ///< Returns App::callee again as App.
    const Pi* callee_type() const { return callee()->type()->as<Pi>(); }
    const Def* arg() const { return op(1); }
    THORIN_PROJ(arg, const)
    ///@}

    /// @name Get axiom, current curry counter and trip count
    ///@{
    const Axiom* axiom() const { return axiom_; }
    u8 curry() const { return curry_; }
    u8 trip() const { return trip_; }
    ///@}

    THORIN_DEF_MIXIN(App)
};

/// @name Helpers to work with Functions
///@{
/// These are Lam%s that are neither `nullptr`, nor Lam::is_external, nor Lam::is_unset.
inline Lam* isa_workable(Lam* lam) {
    if (!lam || lam->is_external() || !lam->is_set()) return nullptr;
    return lam;
}

inline const App* isa_callee(const Def* def, size_t i) { return i == 0 ? def->isa<App>() : nullptr; }
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
/// f:  .Cn[B, .Cn C]
/// g:  .Cn[A, .Cn B]
/// h = f o g
/// h:  .Cn[A, cn C]
/// h = λ (a ret_h) = g (a, h')
/// h': .Cn B
/// h'= λ b = f (b, ret_h)
/// ```
const Def* compose_continuation(const Def* f, const Def* g);
///@}

} // namespace thorin
