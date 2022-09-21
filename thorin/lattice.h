#pragma once

#include "thorin/def.h"

namespace thorin {

class Lam;
class Sigma;

/// Common base for TBound.
class Bound : public Def {
protected:
    /// Constructor for a *structural* Bound.
    Bound(node_t node, const Def* type, Defs ops, const Def* dbg)
        : Def(node, type, ops, 0, dbg) {}
    /// Constructor for a *nom*inal Bound.
    Bound(node_t node, const Def* type, size_t size, const Def* dbg)
        : Def(node, type, size, 0, dbg) {}

public:
    size_t find(const Def* type) const;
    const Def* get(const Def* type) const { return op(find(type)); }
    const Sigma* convert() const;
};

/// Specific [Bound](https://en.wikipedia.org/wiki/Join_and_meet) depending on @p up.
/// The name @p up refers to the property that a [Join](@ref thorin::Join) **ascends** in the underlying
/// [lattice](https://en.wikipedia.org/wiki/Lattice_(order)) while a [Meet](@ref thorin::Meet) descends.
/// * @p up = `true`: [Join](@ref thorin::Join) (aka least upper bound/supremum/union)
/// * @p up = `false`: [Meet](@ref thorin::Meet) (aka greatest lower bound/infimum/intersection)
template<bool up>
class TBound : public Bound {
private:
    /// Constructor for a *structural* Bound.
    TBound(const Def* type, Defs ops, const Def* dbg)
        : Bound(Node, type, ops, dbg) {}
    /// Constructor for a *nom*inal Bound.
    TBound(const Def* type, size_t size, const Def* dbg)
        : Bound(Node, type, size, dbg) {}

public:
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    TBound* stub(World&, const Def*, const Def*) override;
    ///@}

    const Sigma* convert() const;

    static constexpr auto Node = up ? Node::Join : Node::Meet;
    friend class World;
};

/// Constructs a [Meet](@ref thorin::Meet) **value**.
/// @remark [Ac](https://en.wikipedia.org/wiki/Wedge_(symbol)) is Latin and means *and*.
class Ac : public Def {
private:
    Ac(const Def* type, Defs defs, const Def* dbg)
        : Def(Node, type, defs, 0, dbg) {}

public:
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Ac;
    friend class World;
};

/// Constructs a [Join](@ref thorin::Join) **value**.
/// @remark [Vel](https://en.wikipedia.org/wiki/Wedge_(symbol)) is Latin and means *or*.
class Vel : public Def {
private:
    Vel(const Def* type, const Def* value, const Def* dbg)
        : Def(Node, type, {value}, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Vel;
    friend class World;
};

/// Picks the aspect of a Meet [value](Pick::value) by its [type](Def::type).
class Pick : public Def {
private:
    Pick(const Def* type, const Def* value, const Def* dbg)
        : Def(Node, type, {value}, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    ///@}
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Pick;
    friend class World;
};

/// `test value, probe, match, clash` tests whether [value](@ref Test::value) currently holds **type** [probe](@ref
/// Test::probe).
/// @note
/// * [probe](@ref Test::probe) is a **type**!
/// * This operation yields [match](@ref Test::match), if `true`, and [clash](@ref Test::clash) otherwise.
/// @invariant
/// * [value](@ref Test::value) must be of type [Join](@ref thorin::Join).
/// * [match](@ref Test::match) must be of type `A -> B`.
/// * [clash](@ref Test::clash) must be of type `[A, probe] -> C`.
/// @remark This operation is usually known as `case` but named `Test` since `case` is a keyword in C++.
class Test : public Def {
private:
    Test(const Def* type, const Def* value, const Def* probe, const Def* match, const Def* clash, const Def* dbg)
        : Def(Node, type, {value, probe, match, clash}, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    const Def* probe() const { return op(1); }
    const Def* match() const { return op(2); }
    const Def* clash() const { return op(3); }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Test;
    friend class World;
};

/// Common base for TExt%remum.
class Ext : public Def {
protected:
    Ext(node_t node, const Def* type, const Def* dbg)
        : Def(node, type, Defs{}, 0, dbg) {}
};

/// Ext%remum. Either Top (@p up) or Bot%tom.
template<bool up>
class TExt : public Ext {
private:
    TExt(const Def* type, const Def* dbg)
        : Ext(Node, type, dbg) {}

public:
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = up ? Node::Top : Node::Bot;
    friend class World;
};

using Bot  = TExt<false>;
using Top  = TExt<true>;
using Meet = TBound<false>;
using Join = TBound<true>;

inline const Ext* isa_ext(const Def* def) {
    return def->isa<Bot>() || def->isa<Top>() ? static_cast<const Ext*>(def) : nullptr;
}

inline const Bound* isa_bound(const Def* def) {
    return def->isa<Meet>() || def->isa<Join>() ? static_cast<const Bound*>(def) : nullptr;
}

/// A singleton wraps a type into a higher order type.
/// Therefore any type can be the only inhabitant of a singleton.
/// Use in conjunction with @ref thorin::Join.
class Singleton : public Def {
private:
    Singleton(const Def* type, const Def* inner_type, const Def* dbg)
        : Def(Node, type, {inner_type}, 0, dbg) {}

public:
    const Def* inhabitant() const { return op(0); }

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Singleton;
    friend class World;
};

} // namespace thorin
