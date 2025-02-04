#pragma once

#include "mim/def.h"

namespace mim {

/// A [dependent tuple type](https://en.wikipedia.org/wiki/Dependent_type#%CE%A3_type).
/// @see Tuple, Arr, Pack
class Sigma : public Def, public Setters<Sigma> {
private:
    Sigma(const Def* type, Defs ops)
        : Def(Node, type, ops, 0) {} ///< Constructor for an *immutable* Sigma.
    Sigma(const Def* type, size_t size)
        : Def(Node, type, size, 0) {} ///< Constructor for a *mutable* Sigma.

public:
    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Sigma>::set;
    Sigma* set(size_t i, const Def* def) { return Def::set(i, def)->as<Sigma>(); }
    Sigma* set(Defs ops) { return Def::set(ops)->as<Sigma>(); }
    Sigma* unset() { return Def::unset()->as<Sigma>(); }
    ///@}

    /// @name Rebuild
    ///@{
    const Def* immutabilize() override;
    Sigma* stub(Ref type) { return stub_(world(), type)->set(dbg()); }
    ///@}

    /// @name Type Checking
    ///@{
    Ref check(size_t, Ref) override;
    Ref check() override;
    static Ref infer(World&, Defs);
    ///@}

    static constexpr auto Node = Node::Sigma;

private:
    Ref rebuild_(World&, Ref, Defs) const override;
    Sigma* stub_(World&, Ref) override;

    friend class World;
};

/// Data constructor for a Sigma.
/// @see Sigma, Arr, Pack
class Tuple : public Def, public Setters<Tuple> {
public:
    using Setters<Tuple>::set;
    static constexpr auto Node = Node::Tuple;

private:
    Tuple(const Def* type, Defs args)
        : Def(Node, type, args, 0) {}

    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

/// A (possibly paramterized) Arr%ay.
/// Arr%ays are usually homogenous but they can be *inhomogenous* as well: `«i: N; T#i»`
/// @see Sigma, Tuple, Pack
class Arr : public Def, public Setters<Arr> {
private:
    Arr(const Def* type, const Def* shape, const Def* body)
        : Def(Node, type, {shape, body}, 0) {} ///< Constructor for an *immutable* Arr.
    Arr(const Def* type)
        : Def(Node, type, 2, 0) {} ///< Constructor for a *mut*able Arr.

public:
    /// @name ops
    ///@{
    Ref shape() const { return op(0); }
    Ref body() const { return op(1); }
    ///@}

    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Arr>::set;
    Arr* set_shape(Ref shape) { return Def::set(0, shape)->as<Arr>(); }
    Arr* set_body(Ref body) { return Def::set(1, body)->as<Arr>(); }
    Arr* unset() { return Def::unset()->as<Arr>(); }
    ///@}

    /// @name Rebuild
    ///@{
    Ref reduce(Ref arg) const;
    Arr* stub(Ref type) { return stub_(world(), type)->set(dbg()); }
    const Def* immutabilize() override;
    ///@}

    /// @name Type Checking
    ///@{
    Ref check(size_t, Ref) override;
    Ref check() override;
    ///@}

    static constexpr auto Node = Node::Arr;

private:
    Ref rebuild_(World&, Ref, Defs) const override;
    Arr* stub_(World&, Ref) override;

    friend class World;
};

/// A (possibly paramterized) Tuple.
/// @see Sigma, Tuple, Arr
class Pack : public Def, public Setters<Pack> {
private:
    Pack(const Def* type, const Def* body)
        : Def(Node, type, {body}, 0) {} ///< Constructor for an *immutable* Pack.
    Pack(const Def* type)
        : Def(Node, type, 1, 0) {} ///< Constructor for a *mut*ablel Pack.

public:
    /// @name ops
    ///@{
    Ref body() const { return op(0); }
    Ref shape() const;
    ///@}

    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Pack>::set;
    Pack* set(Ref body) { return Def::set(0, body)->as<Pack>(); }
    Pack* reset(Ref body) { return unset()->set(body); }
    Pack* unset() { return Def::unset()->as<Pack>(); }
    ///@}

    /// @name Rebuild
    ///@{
    Ref reduce(Ref arg) const;
    Pack* stub(Ref type) { return stub_(world(), type)->set(dbg()); }
    const Def* immutabilize() override;
    ///@}

    static constexpr auto Node = Node::Pack;

private:
    Ref rebuild_(World&, Ref, Defs) const override;
    Pack* stub_(World&, Ref) override;

    friend class World;
};

/// Extracts from a Sigma or Arr%ay-typed Extract::tuple the element at position Extract::index.
class Extract : public Def, public Setters<Extract> {
private:
    Extract(const Def* type, const Def* tuple, const Def* index)
        : Def(Node, type, {tuple, index}, 0) {}

public:
    using Setters<Extract>::set;

    /// @name ops
    ///@{
    Ref tuple() const { return op(0); }
    Ref index() const { return op(1); }
    ///@}

    static constexpr auto Node = Node::Extract;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

/// Creates a new Tuple / Pack by inserting Insert::value at position Insert::index into Insert::tuple.
/// @attention This is a *functional* Insert.
///     The Insert::tuple itself remains untouched.
///     The Insert itself is a *new* Tuple / Pack which contains the inserted Insert::value.
class Insert : public Def, public Setters<Insert> {
private:
    Insert(const Def* tuple, const Def* index, const Def* value)
        : Def(Node, tuple->type(), {tuple, index, value}, 0) {}

public:
    using Setters<Insert>::set;

    /// @name ops
    ///@{
    Ref tuple() const { return op(0); }
    Ref index() const { return op(1); }
    Ref value() const { return op(2); }
    ///@}

    static constexpr auto Node = Node::Insert;

private:
    Ref rebuild_(World&, Ref, Defs) const override;

    friend class World;
};

/// @name Helpers to work with Tulpes/Sigmas/Arrays/Packs
///@{
bool is_unit(Ref);
std::string tuple2str(Ref);

/// Flattens a sigma/array/pack/tuple.
Ref flatten(Ref def);
/// Same as unflatten, but uses the operands of a flattened Pack / Tuple directly.
size_t flatten(DefVec& ops, Ref def, bool flatten_sigmas = true);

/// Applies the reverse transformation on a Pack / Tuple, given the original type.
Ref unflatten(Ref def, Ref type);
/// Same as unflatten, but uses the operands of a flattened Pack / Tuple directly.
Ref unflatten(Defs ops, Ref type, bool flatten_muts = true);

DefVec merge(Defs, Defs);
DefVec merge(Ref def, Defs defs);
Ref merge_sigma(Ref def, Defs defs);
Ref merge_tuple(Ref def, Defs defs);

Ref tuple_of_types(Ref t);
///@}

} // namespace mim
