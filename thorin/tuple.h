#pragma once

#include "thorin/def.h"

namespace thorin {

/// A [dependent tuple type](https://en.wikipedia.org/wiki/Dependent_type#%CE%A3_type).
/// @see Tuple, Arr, Pack
class Sigma : public Def {
private:
    Sigma(const Def* type, Defs ops)
        : Def(Node, type, ops, 0) {} ///< Constructor for an *immutable* Sigma.
    Sigma(const Def* type, size_t size)
        : Def(Node, type, size, 0) {} ///< Constructor for a *mutable* Sigma.

public:
    /// @name Setters
    ///@{
    /// @see @ref set_ops "Setting Ops"
    Sigma* set(size_t i, const Def* def) { return Def::set(i, def)->as<Sigma>(); }
    Sigma* set(Defs ops) { return Def::set(ops)->as<Sigma>(); }
    Sigma* unset() { return Def::unset()->as<Sigma>(); }
    ///@}

    const Sigma* immutabilize() override;
    Sigma* stub(World&, Ref) override;

    /// @name Type Checking
    ///@{
    void check() override;
    ///@}

    THORIN_DEF_MIXIN(Sigma)
};

/// Data constructor for a Sigma.
/// @see Sigma, Arr, Pack
class Tuple : public Def {
private:
    Tuple(const Def* type, Defs args)
        : Def(Node, type, args, 0) {}

    THORIN_DEF_MIXIN(Tuple)
};

/// A (possibly paramterized) Arr%ay.
/// Arr%ays are usually homogenous but they can be *inhomogenous* as well: `«i: N; T#i»`
/// @see Sigma, Tuple, Pack
class Arr : public Def {
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
    ///@{
    /// @see @ref set_ops "Setting Ops"
    Arr* set_shape(const Def* shape) { return Def::set(0, shape)->as<Arr>(); }
    Arr* set_body(const Def* body) { return Def::set(1, body)->as<Arr>(); }
    Arr* unset() { return Def::unset()->as<Arr>(); }
    ///@}

    const Def* immutabilize() override;
    Arr* stub(World&, Ref) override;
    const Def* reduce(const Def* arg) const;

    /// @name Type Checking
    ///@{
    void check() override;
    ///@}

    THORIN_DEF_MIXIN(Arr)
};

/// A (possibly paramterized) Tuple.
/// @see Sigma, Tuple, Arr
class Pack : public Def {
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
    ///@{
    /// @see @ref set_ops "Setting Ops"
    Pack* set(const Def* body) { return Def::set(0, body)->as<Pack>(); }
    Pack* reset(const Def* body) { return unset()->set(body); }
    Pack* unset() { return Def::unset()->as<Pack>(); }
    ///@}

    const Def* immutabilize() override;
    Pack* stub(World&, Ref) override;
    const Def* reduce(const Def* arg) const;

    THORIN_DEF_MIXIN(Pack)
};

/// Extracts from a Sigma or Arr%ay-typed Extract::tuple the element at position Extract::index.
class Extract : public Def {
private:
    Extract(const Def* type, const Def* tuple, const Def* index)
        : Def(Node, type, {tuple, index}, 0) {}

public:
    /// @name ops
    ///@{
    Ref tuple() const { return op(0); }
    Ref index() const { return op(1); }
    ///@}

    THORIN_DEF_MIXIN(Extract)
};

/// Creates a new Tuple / Pack by inserting Insert::value at position Insert::index into Insert::tuple.
/// @attention This is a *functional* Insert.
///     The Insert::tuple itself remains untouched.
///     The Insert itself is a *new* Tuple / Pack which contains the inserted Insert::value.
class Insert : public Def {
private:
    Insert(const Def* tuple, const Def* index, const Def* value)
        : Def(Node, tuple->type(), {tuple, index, value}, 0) {}

public:
    /// @name ops
    ///@{
    Ref tuple() const { return op(0); }
    Ref index() const { return op(1); }
    Ref value() const { return op(2); }
    ///@}

    THORIN_DEF_MIXIN(Insert)
};

/// @name Helpers to work with Tulpes/Sigmas/Arrays/Packs
///@{
bool is_unit(const Def*);
std::string tuple2str(const Def*);

/// Flattens a sigma/array/pack/tuple.
const Def* flatten(const Def* def);
/// Same as unflatten, but uses the operands of a flattened pack/tuple directly.
size_t flatten(DefVec& ops, const Def* def, bool flatten_sigmas = true);

/// Applies the reverse transformation on a pack/tuple, given the original type.
const Def* unflatten(const Def* def, const Def* type);
/// Same as unflatten, but uses the operands of a flattened pack/tuple directly.
const Def* unflatten(Defs ops, const Def* type, bool flatten_muts = true);

DefArray merge(Defs, Defs);
DefArray merge(const Def* def, Defs defs);
const Def* merge_sigma(const Def* def, Defs defs);
const Def* merge_tuple(const Def* def, Defs defs);

Ref tuple_of_types(Ref t);
///@}

} // namespace thorin
