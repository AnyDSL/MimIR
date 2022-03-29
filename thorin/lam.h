#ifndef THORIN_LAM_H
#define THORIN_LAM_H

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
    const Def* codom() const { return op(1); }
    THORIN_PROJ(dom, const)
    THORIN_PROJ(codom, const)
    bool is_cn() const;
    bool is_basicblock() const { return is_cn() && !ret_pi(); }
    bool is_returning() const { return is_cn() && ret_pi(); }
    const Pi* ret_pi(const Def* dbg = {}) const;
    ///@}

    /// @name setters for *nom*inal Pi.
    ///@{
    Pi* set_dom(const Def* dom) { return Def::set(0, dom)->as<Pi>(); }
    Pi* set_dom(Defs doms);
    Pi* set_codom(const Def* codom) { return Def::set(1, codom)->as<Pi>(); }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    Pi* stub(World&, const Def*, const Def*) override;
    const Pi* restructure() override;
    ///@}

    static constexpr auto Node = Node::Pi;
    friend class World;
};

class Lam : public Def {
public:
    /// calling convention
    enum class CC : u8 {
        C,      ///< C calling convention.
        Device, ///< Device calling convention. These are special functions only available on a particular device.
    };

private:
    Lam(const Pi* pi, const Def* filter, const Def* body, const Def* dbg)
        : Def(Node, pi, {filter, body}, 0, dbg) {}
    Lam(const Pi* pi, CC cc, const Def* dbg)
        : Def(Node, pi, 2, u64(cc), dbg) {}

public:
    /// @name type
    ///@{
    const Pi* type() const { return Def::type()->as<Pi>(); }
    bool is_basicblock() const { return type()->is_basicblock(); }
    bool is_returning() const { return type()->is_returning(); }
    const Def* dom() const { return type()->dom(); }
    const Def* codom() const { return type()->codom(); }
    const Pi* ret_pi() const { return type()->ret_pi(); }
    THORIN_PROJ(dom, const)
    THORIN_PROJ(codom, const)
    ///@}

    /// @name ops
    ///@{
    const Def* filter() const { return op(0); }
    const Def* body() const { return op(1); }
    ///@}

    /// @name vars
    ///@{
    const Def* mem_var(const Def* dbg = {});
    const Def* ret_var(const Def* dbg = {});
    ///@}

    /// @name Setters for nominal Lam.
    ///@{
    /// Lam::Filter is a `std::variant<bool, const Def*>` that lets you set the Lam::filter() like this:
    /// ```cpp
    /// lam1->app(true, f, arg);
    /// lam2->app(my_filter_def, f, arg);
    /// ```
    using Filter = std::variant<bool, const Def*>;
    Lam* set(size_t i, const Def* def) { return Def::set(i, def)->as<Lam>(); }
    Lam* set(Defs ops) { return Def::set(ops)->as<Lam>(); }
    Lam* set(Filter filter, const Def* body) {
        set_filter(filter);
        return set_body(body);
    }
    Lam* set_filter(Filter);
    Lam* set_body(const Def* body) { return set(1, body); }
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
    ///@}

    /// @name get/set fields - CC
    ///@{
    CC cc() const { return CC(fields()); }
    void set_cc(CC cc) { fields_ = u64(cc); }
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
    App(const Axiom* axiom, u16 curry, const Def* type, const Def* callee, const Def* arg, const Def* dbg)
        : Def(Node, type, {callee, arg}, 0, dbg) {
        axiom_ = axiom;
        curry_ = curry;
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

    /// @name get axiom and current currying depth
    ///@{
    const Axiom* axiom() const { return axiom_; }
    u16 curry() const { return curry_; }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::App;
    friend class World;
};

inline Stream& operator<<(Stream& s, std::pair<Lam*, Lam*> p) {
    return operator<<(s, std::pair<const Def*, const Def*>(p));
}

/// These are Lam%s that are neither `nullptr`, nor Lam::is_external, nor Lam::is_unset.
inline Lam* isa_workable(Lam* lam) {
    if (!lam || lam->is_external() || lam->is_unset()) return nullptr;
    return lam;
}

inline const App* isa_callee(const Def* def, size_t i) { return i == 0 ? def->isa<App>() : nullptr; }
inline std::pair<const App*, Lam*> isa_apped_nom_lam(const Def* def) {
    if (auto app = def->isa<App>()) return {app, app->callee()->isa_nom<Lam>()};
    return {nullptr, nullptr};
}

} // namespace thorin

#endif
