#pragma once

#include "mim/def.h"

namespace mim {

class Lam;
class Sigma;

/// Common base for TBound.
class Bound : public Def {
protected:
    Bound(Node node, const Def* type, Defs ops)
        : Def(node, type, ops, 0) {}

    constexpr size_t reduction_offset() const noexcept override { return 0; }

public:
    /// @name Get Element by Type
    ///@{
    size_t find(const Def* type) const;
    const Def* get(const Def* type) const { return op(find(type)); }
    ///@}
};

/// Specific [Bound](https://en.wikipedia.org/wiki/Join_and_meet) depending on @p Up.
/// The name @p Up refers to the property that a [Join](@ref mim::Join) **ascends** in the underlying
/// [lattice](https://en.wikipedia.org/wiki/Lattice_(order)) while a [Meet](@ref mim::Meet) descends.
/// * @p Up = `true`: [Join](@ref mim::Join) (aka least Upper bound/supremum/union)
/// * @p Up = `false`: [Meet](@ref mim::Meet) (aka greatest lower bound/infimum/intersection)
template<bool Up> class TBound : public Bound, public Setters<TBound<Up>> {
private:
    TBound(const Def* type, Defs ops)
        : Bound(Node, type, ops) {}

public:
    using Setters<TBound<Up>>::set;

    static constexpr auto Node = Up ? mim::Node::Join : mim::Node::Meet;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// Constructs a [Meet](@ref mim::Meet) **value**.
/// @remark [Ac](https://en.wikipedia.org/wiki/Wedge_(symbol)) is Latin and means *and*.
class Merge : public Def, public Setters<Merge> {
public:
    using Setters<Merge>::set;
    static constexpr auto Node = mim::Node::Ac;

private:
    Merge(const Def* type, Defs defs)
        : Def(Node, type, defs, 0) {}

    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// Constructs a [Join](@ref mim::Join) **value**.
/// @remark [Inj](https://en.wikipedia.org/wiki/Wedge_(symbol)) is Latin and means *or*.
class Inj : public Def, public Setters<Inj> {
private:
    Inj(const Def* type, const Def* value)
        : Def(Node, type, {value}, 0) {}

public:
    using Setters<Inj>::set;

    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    ///@}

    static constexpr auto Node = mim::Node::Inj;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// Picks the aspect of a Meet [value](Pick::value) by its [type](Def::type).
class Split : public Def, public Setters<Split> {
private:
    Split(const Def* type, const Def* value)
        : Def(Node, type, {value}, 0) {}

public:
    using Setters<Split>::set;

    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    ///@}

    static constexpr auto Node = mim::Node::Pick;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// Test whether Test::value currently holds **type** Test::probe:
/// ```
/// test value, probe, match, clash
/// ```
/// @note
/// * Test::probe is a **type**!
/// * This operation yields Test::match, if `tt`, and Test::clash otherwise.
/// @invariant
/// * Test::value must be of type Join.
/// * Test::match must be of type `A -> B`.
/// * Test::clash must be of type `[A, probe] -> C`.
/// @remark This operation is usually known as `case` but named `Test` since `case` is a keyword in C++.
class Test : public Def, public Setters<Test> {
private:
    Test(const Def* type, const Def* value, const Def* probe, const Def* match, const Def* clash)
        : Def(Node, type, {value, probe, match, clash}, 0) {}

public:
    using Setters<Test>::set;
    static constexpr auto Node = mim::Node::Test;

    /// @name ops
    ///@{
    const Def* value() const { return op(0); }
    const Def* probe() const { return op(1); }
    const Def* match() const { return op(2); }
    const Def* clash() const { return op(3); }
    ///@}

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// Common base for TExt%remum.
class Ext : public Def {
protected:
    Ext(Node node, const Def* type)
        : Def(node, type, Defs{}, 0) {}
};

/// Ext%remum. Either Top (@p Up) or Bot%tom.
template<bool Up> class TExt : public Ext, public Setters<TExt<Up>> {
private:
    TExt(const Def* type)
        : Ext(Node, type) {}

public:
    using Setters<TExt<Up>>::set;

    static constexpr auto Node = Up ? mim::Node::Top : mim::Node::Bot;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// @name Lattice
///@{
using Bot  = TExt<false>;
using Top  = TExt<true>;
using Meet = TBound<false>; ///< AKA intersection.
using Join = TBound<true>;  ///< AKA union.
/// @}

/// A singleton wraps a type into a higher order type.
/// Therefore any type can be the only inhabitant of a singleton.
/// Use in conjunction with @ref mim::Join.
class Uniq : public Def, public Setters<Uniq> {
private:
    Uniq(const Def* type, const Def* inner_type)
        : Def(Node, type, {inner_type}, 0) {}

public:
    using Setters<Uniq>::set;

    /// @name ops
    ///@{
    const Def* inhabitant() const { return op(0); }
    ///@}

    static constexpr auto Node = mim::Node::Uniq;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

} // namespace mim
