#pragma once

#include "thorin/def.h"

namespace thorin {

class Lam;
class Sigma;

/// Common base for TBound.
class Bound : public Def {
protected:
    Bound(node_t node, const Def* type, Defs ops)
        : Def(node, type, ops, 0) {} ///< Constructor for an *immutable* Bound.
    Bound(node_t node, const Def* type, size_t size)
        : Def(node, type, size, 0) {} ///< Constructor for a *mutable* Bound.

public:
    size_t find(const Def* type) const;
    const Def* get(const Def* type) const { return op(find(type)); }
};

/// Specific [Bound](https://en.wikipedia.org/wiki/Join_and_meet) depending on @p Up.
/// The name @p Up refers to the property that a [Join](@ref thorin::Join) **ascends** in the underlying
/// [lattice](https://en.wikipedia.org/wiki/Lattice_(order)) while a [Meet](@ref thorin::Meet) descends.
/// * @p Up = `true`: [Join](@ref thorin::Join) (aka least Upper bound/supremum/union)
/// * @p Up = `false`: [Meet](@ref thorin::Meet) (aka greatest lower bound/infimum/intersection)
template<bool Up>
class TBound : public Bound {
private:
    TBound(const Def* type, Defs ops)
        : Bound(Node, type, ops) {} ///< Constructor for an *immutable* Bound.
    TBound(const Def* type, size_t size)
        : Bound(Node, type, size) {} ///< Constructor for a *mutable* Bound.

    THORIN_SETTERS(TBound)
    TBound* stub(World&, Ref) override;

    static constexpr auto Node = Up ? Node::Join : Node::Meet;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

/// Constructs a [Meet](@ref thorin::Meet) **value**.
/// @remark [Ac](https://en.wikipedia.org/wiki/Wedge_(symbol)) is Latin and means *and*.
class Ac : public Def {
private:
    Ac(const Def* type, Defs defs)
        : Def(Node, type, defs, 0) {}

    THORIN_DEF_MIXIN(Ac)
};

/// Constructs a [Join](@ref thorin::Join) **value**.
/// @remark [Vel](https://en.wikipedia.org/wiki/Wedge_(symbol)) is Latin and means *or*.
class Vel : public Def {
private:
    Vel(const Def* type, const Def* value)
        : Def(Node, type, {value}, 0) {}

public:
    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    ///@}

    THORIN_DEF_MIXIN(Vel)
};

/// Picks the aspect of a Meet [value](Pick::value) by its [type](Def::type).
class Pick : public Def {
private:
    Pick(const Def* type, const Def* value)
        : Def(Node, type, {value}, 0) {}

public:
    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    ///@}

    THORIN_DEF_MIXIN(Pick)
};

/// `test value, probe, match, clash` tests whether Test::value currently holds **type** Test::probe.
/// @note
/// * Test::probe is a **type**!
/// * This operation yields Test::match, if `tt`, and Test::clash otherwise.
/// @invariant
/// * Test::value must be of type Join.
/// * Test::match must be of type `A -> B`.
/// * Test::clash must be of type `[A, probe] -> C`.
/// @remark This operation is usually known as `case` but named `Test` since `case` is a keyword in C++.
class Test : public Def {
private:
    Test(const Def* type, const Def* value, const Def* probe, const Def* match, const Def* clash)
        : Def(Node, type, {value, probe, match, clash}, 0) {}

public:
    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    const Def* probe() const { return op(1); }
    const Def* match() const { return op(2); }
    const Def* clash() const { return op(3); }
    ///@}

    THORIN_DEF_MIXIN(Test)
};

/// Common base for TExt%remum.
class Ext : public Def {
protected:
    Ext(node_t node, const Def* type)
        : Def(node, type, Defs{}, 0) {}
};

/// Ext%remum. Either Top (@p Up) or Bot%tom.
template<bool Up>
class TExt : public Ext {
private:
    TExt(const Def* type)
        : Ext(Node, type) {}

    THORIN_SETTERS(TExt)
    TExt* stub(World&, Ref) override;
    static constexpr auto Node = Up ? Node::Top : Node::Bot;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

using Bot  = TExt<false>;
using Top  = TExt<true>;
using Meet = TBound<false>;
using Join = TBound<true>;

/// A singleton wraps a type into a higher order type.
/// Therefore any type can be the only inhabitant of a singleton.
/// Use in conjunction with @ref thorin::Join.
class Singleton : public Def {
private:
    Singleton(const Def* type, const Def* inner_type)
        : Def(Node, type, {inner_type}, 0) {}

public:
    const Def* inhabitant() const { return op(0); }

    THORIN_DEF_MIXIN(Singleton)
};

} // namespace thorin
