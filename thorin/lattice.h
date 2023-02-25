#pragma once

#include "thorin/def.h"

namespace thorin {

class Lam;
class Sigma;

/// Common base for TBound.
class Bound : public Def {
protected:
    /// Constructor for a *structural* Bound.
    Bound(node_t node, const Def* type, Defs ops)
        : Def(node, type, ops, 0) {}
    /// Constructor for a *nom*inal Bound.
    Bound(node_t node, const Def* type, size_t size)
        : Def(node, type, size, 0) {}

public:
    size_t find(const Def* type) const;
    const Def* get(const Def* type) const { return op(find(type)); }
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
    TBound(const Def* type, Defs ops)
        : Bound(Node, type, ops) {}
    /// Constructor for a *nom*inal Bound.
    TBound(const Def* type, size_t size)
        : Bound(Node, type, size) {}

    THORIN_DEF_MIXIN(TBound<up>, ;, up ? Node::Join : Node::Meet)
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

/// Ext%remum. Either Top (@p up) or Bot%tom.
template<bool up>
class TExt : public Ext {
private:
    TExt(const Def* type)
        : Ext(Node, type) {}

    THORIN_DEF_MIXIN(
        TExt<up>,
        { unreachable(); },
        up ? Node::Top : Node::Bot)
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
