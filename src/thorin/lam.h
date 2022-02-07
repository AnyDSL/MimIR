#ifndef THORIN_LAM_H
#define THORIN_LAM_H

#include "thorin/def.h"

namespace thorin {

/// A function type AKA Pi type.
class Pi : public Def {
protected:
    /// Constructor for a @em structural Pi.
    Pi(const Def* type, const Def* dom, const Def* codom, const Def* dbg)
        : Def(Node, type, {dom, codom}, 0, dbg)
    {}
    /// Constructor for a @em nom Pi.
    Pi(const Def* type, const Def* dbg)
        : Def(Node, type, 2, 0, dbg)
    {}

public:
    /// @name ops
    //@{
    const Def* dom() const { return op(0); }
    const Def* codom() const { return op(1); }
    THORIN_PROJ(dom, const)
    THORIN_PROJ(codom, const)
    bool is_cn() const;
    bool is_basicblock() const { return order() == 1; }
    bool is_returning() const;
    const Pi* ret_pi(const Def* dbg = {}) const;
    //@}

    /// @name setters for @em nom @p Pi.
    //@{
    Pi* set_dom(const Def* dom) { return Def::set(0, dom)->as<Pi>(); }
    Pi* set_dom(Defs doms);
    Pi* set_codom(const Def* codom) { return Def::set(1, codom)->as<Pi>(); }
    //@}

    /// @name virtual methods
    //@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    Pi* stub(World&, const Def*, const Def*) override;
    const Pi* restructure() override;
    //@}

    static constexpr auto Node = Node::Pi;
    friend class World;
};

class Lam : public Def {
public:
    /// calling convention
    enum class CC : u8 {
        C,          ///< C calling convention.
        Device,     ///< Device calling convention. These are special functions only available on a particular device.
    };

private:
    Lam(const Pi* pi, const Def* filter, const Def* body, const Def* dbg)
        : Def(Node, pi, {filter, body}, 0, dbg)
    {}
    Lam(const Pi* pi, CC cc, const Def* dbg)
        : Def(Node, pi, 2, u64(cc), dbg)
    {}

public:
    /// @name type
    //@{
    const Pi* type() const { return Def::type()->as<Pi>(); }
    const Def* dom() const { return type()->dom(); }
    const Def* codom() const { return type()->codom(); }
    THORIN_PROJ(dom, const)
    THORIN_PROJ(codom, const)
    //@}

    /// @name ops
    //@{
    const Def* filter() const { return op(0); }
    const Def* body() const { return op(1); }
    //@}

    /// @name vars
    //@{
    const Def* mem_var(const Def* dbg = {});
    const Def* ret_var(const Def* dbg = {});
    //@}

    /// @name setters
    //@{
    Lam* set(size_t i, const Def* def) { return Def::set(i, def)->as<Lam>(); }
    Lam* set(Defs ops) { return Def::set(ops)->as<Lam>(); }
    Lam* set(const Def* filter, const Def* body) { return set({filter, body}); }
    Lam* set_filter(const Def* filter) { return set(0_s, filter); }
    Lam* set_filter(bool filter);
    Lam* set_body(const Def* body) { return set(1, body); }
    //@}

    /// @name setters: sets filter to @c false and sets the body by @p App -ing
    //@{
    void app(const Def* callee, const Def* arg, const Def* dbg = {});
    void app(const Def* callee, Defs args, const Def* dbg = {});
    void branch(const Def* cond, const Def* t, const Def* f, const Def* mem, const Def* dbg = {});
    void test(const Def* value, const Def* index, const Def* match, const Def* clash, const Def* mem, const Def* dbg = {});
    //@}

    /// @name virtual methods
    //@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    Lam* stub(World&, const Def*, const Def*) override;
    //@}

    /// @name get/set fields - CC
    //@{
    CC cc() const { return CC(fields()); }
    void set_cc(CC cc) { fields_ = u64(cc); }
    //@}

    bool is_basicblock() const;

    static constexpr auto Node = Node::Lam;
    friend class World;
};

template<class To>
using LamMap  = GIDMap<Lam*, To>;
using LamSet  = GIDSet<Lam*>;
using Lam2Lam = LamMap<Lam*>;

class App : public Def {
private:
    App(const Axiom* axiom, u16 currying_depth, const Def* type, const Def* callee, const Def* arg, const Def* dbg)
        : Def(Node, type, {callee, arg}, 0, dbg)
    {
        axiom_depth_.set(axiom, currying_depth);
    }

public:
    /// @name ops
    ///@{
    const Def* callee() const { return op(0); }
    const App* decurry() const { return callee()->as<App>(); } ///< Returns the @p callee again as @p App.
    const Pi* callee_type() const { return callee()->type()->as<Pi>(); }
    const Def* arg() const { return op(1); }
    THORIN_PROJ(arg, const)
    //@}

    /// @name get axiom and current currying depth
    //@{
    const Axiom* axiom() const { return axiom_depth_.ptr(); }
    u16 currying_depth() const { return axiom_depth_.index(); }
    //@}

    /// @name virtual methods
    //@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    //@}

    static constexpr auto Node = Node::App;
    friend class World;
};

inline const App* isa_callee(const Def* def, size_t i) { return i == 0 ? def->isa<App>() : nullptr; }

inline Stream& operator<<(Stream& s, std::pair<Lam*, Lam*> p) { return operator<<(s, std::pair<const Def*, const Def*>(p.first, p.second)); }

// TODO remove - deprecated
Lam* get_var_lam(const Def* def);

// TODO remove - deprecated: This one is more confusing than helping.
inline bool ignore(Lam* lam) { return lam == nullptr || lam->is_external() || !lam->is_set(); }

}

#endif
