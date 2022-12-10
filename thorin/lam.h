#pragma once

#include <variant>

#include "thorin/def.h"

namespace thorin {

/// A function type AKA Pi type.
class Pi : public Def {
protected:
    /// Constructor for a *structural* Pi.
    Pi(const Def* type, const Def* dom, const Def* codom, const Def* dbg)
        : Def(Node, type, {dom, codom}, 0, dbg) {}
    /// Constructor for a *nom*inal Pi.
    Pi(const Def* type, const Def* dbg)
        : Def(Node, type, 2, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* dom() const { return op(0); }
    THORIN_PROJ(dom, const)
    const Def* codom() const { return op(1); }
    THORIN_PROJ(codom, const)
    bool is_cn() const;
    bool is_basicblock() const { return is_cn() && !ret_pi(); }
    bool is_returning() const { return is_cn() && ret_pi(); }
    const Pi* ret_pi(const Def* dbg = {}) const;
    ///@}

    /// @name setters
    ///@{
    Pi* set_dom(const Def* dom) { return Def::set(0, dom)->as<Pi>(); }
    Pi* set_dom(Defs doms);
    Pi* set_codom(const Def* codom) { return Def::set(1, codom)->as<Pi>(); }
    ///@}

    static const Def* infer(const Def* dom, const Def* codom);

    /// @name virtual methods
    ///@{
    size_t first_dependend_op() { return 1; }
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    Pi* stub(World&, const Def*, const Def*) override;
    const Pi* restructure() override;
    void check() override;
    ///@}

    static constexpr auto Node = Node::Pi;
    friend class World;
};

class Lam : public Def {
private:
    Lam(const Pi* pi, const Def* filter, const Def* body, const Def* dbg)
        : Def(Node, pi, {filter, body}, 0, dbg) {}
    Lam(const Pi* pi, const Def* dbg)
        : Def(Node, pi, 2, 0, dbg) {}

public:
    /// @name type
    ///@{
    const Pi* type() const { return Def::type()->as<Pi>(); }
    const Def* dom() const { return type()->dom(); }
    THORIN_PROJ(dom, const)
    const Def* codom() const { return type()->codom(); }
    THORIN_PROJ(codom, const)
    bool is_basicblock() const { return type()->is_basicblock(); }
    bool is_returning() const { return type()->is_returning(); }
    const Pi* ret_pi() const { return type()->ret_pi(); }
    ///@}

    /// @name ops
    ///@{
    const Def* filter() const { return op(0); }
    const Def* body() const { return op(1); }
    ///@}

    /// @name vars
    ///@{
    const Def* ret_var(const Def* dbg = {});
    ///@}

    /// @name setters
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
    Lam* app(Filter filter, const Def* callee, const Def* arg, const Def* dbg = {});
    /// Set body to an App of @p callee and @p args.
    Lam* app(Filter filter, const Def* callee, Defs args, const Def* dbg = {});
    /// Set body to an App of `(f, t)#cond mem`.
    Lam* branch(Filter filter, const Def* cond, const Def* t, const Def* f, const Def* mem, const Def* dbg = {});
    Lam* test(Filter filter,
              const Def* val,
              const Def* idx,
              const Def* match,
              const Def* clash,
              const Def* mem,
              const Def* dbg = {});
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    Lam* stub(World&, const Def*, const Def*) override;
    void check() override;
    ///@}

    static constexpr auto Node = Node::Lam;
    friend class World;
};

template<class To>
using LamMap  = GIDMap<Lam*, To>;
using LamSet  = GIDSet<Lam*>;
using Lam2Lam = LamMap<Lam*>;

class App : public Def {
private:
    App(const Axiom* axiom, u8 curry, u8 trip, const Def* type, const Def* callee, const Def* arg, const Def* dbg)
        : Def(Node, type, {callee, arg}, 0, dbg) {
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

    /// @name get axiom, current curry counter and trip count
    ///@{
    const Axiom* axiom() const { return axiom_; }
    u8 curry() const { return curry_; }
    u8 trip() const { return trip_; }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::App;
    friend class World;
};

/// These are Lam%s that are neither `nullptr`, nor Lam::is_external, nor Lam::is_unset.
inline Lam* isa_workable(Lam* lam) {
    if (!lam || lam->is_external() || !lam->is_set()) return nullptr;
    return lam;
}

inline const App* isa_callee(const Def* def, size_t i) { return i == 0 ? def->isa<App>() : nullptr; }
inline std::pair<const App*, Lam*> isa_apped_nom_lam(const Def* def) {
    if (auto app = def->isa<App>()) return {app, app->callee()->isa_nom<Lam>()};
    return {nullptr, nullptr};
}

/// Yields curried App%s in a flat `std::deque<const App*>`.
std::deque<const App*> decurry(const Def*);

} // namespace thorin
