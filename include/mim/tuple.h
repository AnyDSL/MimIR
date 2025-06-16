#pragma once

#include "mim/def.h"

namespace mim {

/// Base class for Sigma and Tuple.
class Prod : public Def, public Setters<Prod> {
protected:
    using Def::Def;
};

/// A [dependent tuple type](https://en.wikipedia.org/wiki/Dependent_type#%CE%A3_type).
/// @see Tuple, Arr, Pack, Prod
class Sigma : public Prod, public Setters<Sigma> {
private:
    Sigma(const Def* type, Defs ops)
        : Prod(Node, type, ops, 0) {} ///< Constructor for an *immutable* Sigma.
    Sigma(const Def* type, size_t size)
        : Prod(Node, type, size, 0) {} ///< Constructor for a *mutable* Sigma.

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
    Sigma* stub(const Def* type) { return stub_(world(), type)->set(dbg()); }

    /// @note Technically, it would make sense to have an offset of 1 as the first element can't be reduced.
    /// For example, in `[n: Nat, F n]` `n` only occurs free in the second element.
    /// However, this would cause a lot of confusion and special code to cope with the first element,
    /// So we just keep it.
    constexpr size_t reduction_offset() const noexcept override { return 0; }
    ///@}

    /// @name Type Checking
    ///@{
    const Def* check(size_t, const Def*) override;
    const Def* check() override;
    static const Def* infer(World&, Defs);
    ///@}

    static constexpr auto Node = mim::Node::Sigma;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;
    Sigma* stub_(World&, const Def*) override;

    friend class World;
};

/// Data constructor for a Sigma.
/// @see Sigma, Arr, Pack, Prod
class Tuple : public Prod, public Setters<Tuple> {
public:
    using Setters<Tuple>::set;
    static const Def* infer(World&, Defs);
    static constexpr auto Node = mim::Node::Tuple;

private:
    Tuple(const Def* type, Defs args)
        : Prod(Node, type, args, 0) {}

    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// Base class for Arr and Pack.
class Seq : public Def, public Setters<Seq> {
protected:
    using Def::Def;

public:
    /// @name ops
    ///@{
    virtual const Def* shape() const = 0;
    const Def* body() const { return ops().back(); }
    ///@}

    /// @name Rebuild
    ///@{
    Seq* stub(World& w, const Def* type) { return Def::stub(w, type)->as<Seq>(); }
    virtual const Def* rebuild(World&, const Def* shape, const Def* body) const = 0;
    virtual const Def* prod(World& w, Defs) const = 0; ///< Creates either a Tuple or Sigma.
    ///@}
};

/// A (possibly paramterized) Arr%ay.
/// Arr%ays are usually homogenous but they can be *inhomogenous* as well: `«i: N; T#i»`
/// @see Sigma, Tuple, Pack
class Arr : public Seq, public Setters<Arr> {
private:
    Arr(const Def* type, const Def* shape, const Def* body)
        : Seq(Node, type, {shape, body}, 0) {} ///< Constructor for an *immutable* Arr.
    Arr(const Def* type)
        : Seq(Node, type, 2, 0) {} ///< Constructor for a *mutable* Arr.

public:
    /// @name ops
    ///@{
    const Def* shape() const final { return op(0); }
    ///@}

    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Arr>::set;
    Arr* set_shape(const Def* shape) { return Def::set(0, shape)->as<Arr>(); }
    Arr* set_body(const Def* body) { return Def::set(1, body)->as<Arr>(); }
    Arr* set(const Def* shape, const Def* body) { return set_shape(shape)->set_body(body); }
    Arr* unset() { return Def::unset()->as<Arr>(); }
    ///@}

    /// @name Rebuild
    ///@{
    const Def* rebuild(World& w, const Def* shape, const Def* body) const override;
    Arr* stub(const Def* type) { return stub_(world(), type)->set(dbg()); }
    const Def* immutabilize() override;
    const Def* reduce(const Def* arg) const { return Def::reduce(arg).front(); }
    constexpr size_t reduction_offset() const noexcept override { return 1; }
    const Def* prod(World&, Defs) const final;
    ///@}

    /// @name Type Checking
    ///@{
    const Def* check(size_t, const Def*) override;
    const Def* check() override;
    ///@}

    static constexpr auto Node = mim::Node::Arr;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;
    Arr* stub_(World&, const Def*) override;

    friend class World;
};

/// A (possibly paramterized) Tuple.
/// @see Sigma, Tuple, Arr
class Pack : public Seq, public Setters<Pack> {
private:
    Pack(const Def* type, const Def* body)
        : Seq(Node, type, {body}, 0) {} ///< Constructor for an *immutable* Pack.
    Pack(const Def* type)
        : Seq(Node, type, 1, 0) {} ///< Constructor for a *mutable* Pack.

public:
    /// @name ops
    ///@{
    const Def* shape() const final;
    ///@}

    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Pack>::set;
    Pack* set(const Def* body) { return Def::set(0, body)->as<Pack>(); }
    Pack* reset(const Def* body) { return unset()->set(body); }
    Pack* unset() { return Def::unset()->as<Pack>(); }
    ///@}

    /// @name Rebuild
    ///@{
    const Def* rebuild(World& w, const Def* shape, const Def* body) const override;
    Pack* stub(const Def* type) { return stub_(world(), type)->set(dbg()); }
    const Def* immutabilize() override;
    const Def* reduce(const Def* arg) const { return Def::reduce(arg).front(); }
    constexpr size_t reduction_offset() const noexcept override { return 0; }
    const Def* prod(World&, Defs) const final;
    ///@}

    static constexpr auto Node = mim::Node::Pack;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;
    Pack* stub_(World&, const Def*) override;

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
    const Def* tuple() const { return op(0); }
    const Def* index() const { return op(1); }
    ///@}

    static constexpr auto Node = mim::Node::Extract;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

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
    const Def* tuple() const { return op(0); }
    const Def* index() const { return op(1); }
    const Def* value() const { return op(2); }
    ///@}

    static constexpr auto Node = mim::Node::Insert;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// @name Helpers to work with Tulpes/Sigmas/Arrays/Packs
///@{
bool is_unit(const Def*);
std::string tuple2str(const Def*);

/// Flattens a sigma/array/pack/tuple.
const Def* flatten(const Def* def);
/// Same as unflatten, but uses the operands of a flattened Pack / Tuple directly.
size_t flatten(DefVec& ops, const Def* def, bool flatten_sigmas = true);

/// Applies the reverse transformation on a Pack / Tuple, given the original type.
const Def* unflatten(const Def* def, const Def* type);
/// Same as unflatten, but uses the operands of a flattened Pack / Tuple directly.
const Def* unflatten(Defs ops, const Def* type, bool flatten_muts = true);

DefVec merge(Defs, Defs);
DefVec merge(const Def* def, Defs defs);
const Def* merge_sigma(const Def* def, Defs defs);
const Def* merge_tuple(const Def* def, Defs defs);

const Def* tuple_of_types(const Def* t);
///@}

} // namespace mim
