#pragma once

#include "thorin/def.h"

namespace thorin {

class Sigma : public Def {
private:
    /// Constructor for a *structural* Sigma.
    Sigma(const Def* type, Defs ops, const Def* dbg)
        : Def(Node, type, ops, 0, dbg) {}
    /// Constructor for a *nom*inal Sigma.
    Sigma(const Def* type, size_t size, const Def* dbg)
        : Def(Node, type, size, 0, dbg) {}

public:
    /// @name setters
    ///@{
    Sigma* set(size_t i, const Def* def) { return Def::set(i, def)->as<Sigma>(); }
    Sigma* set(Defs ops) { return Def::set(ops)->as<Sigma>(); }
    ///@}

    /// @name virtual methods
    ///@{
    void check() override;
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    Sigma* stub(World&, const Def*, const Def*) override;
    const Sigma* restructure() override;
    ///@}

    static constexpr auto Node = Node::Sigma;
    friend class World;
};

/// Data constructor for a Sigma.
class Tuple : public Def {
private:
    Tuple(const Def* type, Defs args, const Def* dbg)
        : Def(Node, type, args, 0, dbg) {}

public:
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Tuple;
    friend class World;
};

class Arr : public Def {
private:
    /// Constructor for a *structural* Arr.
    Arr(const Def* type, const Def* shape, const Def* body, const Def* dbg)
        : Def(Node, type, {shape, body}, 0, dbg) {}
    /// Constructor for a *nom*inaml Arr.
    Arr(const Def* type, const Def* dbg)
        : Def(Node, type, 2, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* shape() const { return op(0); }
    const Def* body() const { return op(1); }
    Arr* set_shape(const Def* shape) { return Def::set(0, shape)->as<Arr>(); }
    Arr* set_body(const Def* body) { return Def::set(1, body)->as<Arr>(); }
    ///@}

    const Def* reduce(const Def* arg) const;

    /// @name virtual methods
    ///@{
    size_t first_dependend_op() override { return 1; }
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    Arr* stub(World&, const Def*, const Def*) override;
    const Def* restructure() override;
    void check() override;
    ///@}

    static constexpr auto Node = Node::Arr;
    friend class World;
};

class Pack : public Def {
private:
    /// Constructor for a *structural* Pack.
    Pack(const Def* type, const Def* body, const Def* dbg)
        : Def(Node, type, {body}, 0, dbg) {}
    /// Constructor for a *nom*inaml Pack.
    Pack(const Def* type, const Def* dbg)
        : Def(Node, type, 1, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* body() const { return op(0); }
    const Arr* type() const { return Def::type()->as<Arr>(); }
    const Def* shape() const { return type()->shape(); }
    Pack* set(const Def* body) { return Def::set(0, body)->as<Pack>(); }
    ///@}

    const Def* reduce(const Def* arg) const;

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    Pack* stub(World&, const Def*, const Def*) override;
    const Def* restructure() override;
    ///@}

    static constexpr auto Node = Node::Pack;
    friend class World;
};

/// Extracts from a Sigma or Arr-typed Extract::tuple the element at position Extract::index.
class Extract : public Def {
private:
    Extract(const Def* type, const Def* tuple, const Def* index, const Def* dbg)
        : Def(Node, type, {tuple, index}, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* tuple() const { return op(0); }
    const Def* index() const { return op(1); }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Extract;
    friend class World;
};

/// Creates a new Tuple / Pack by inserting Insert::value at position Insert::index into Insert::tuple.
/// @attention This is a *functional* Insert.
///     The Insert::tuple itself remains untouched.
///     The Insert itself is a *new* Tuple / Pack which contains the inserted Insert::value.
class Insert : public Def {
private:
    Insert(const Def* tuple, const Def* index, const Def* value, const Def* dbg)
        : Def(Node, tuple->type(), {tuple, index, value}, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* tuple() const { return op(0); }
    const Def* index() const { return op(1); }
    const Def* value() const { return op(2); }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::Insert;
    friend class World;
};

/// Flattens a sigma/array/pack/tuple.
const Def* flatten(const Def* def);
size_t flatten(DefVec& ops, const Def* def, bool flatten_sigmas = true);

/// Applies the reverse transformation on a pack/tuple, given the original type.
const Def* unflatten(const Def* def, const Def* type);
/// Same as unflatten, but uses the operands of a flattened pack/tuple directly.
const Def* unflatten(Defs ops, const Def* type, bool flatten_noms = true);

DefArray merge(const Def* def, Defs defs);
const Def* merge_sigma(const Def* def, Defs defs);
const Def* merge_tuple(const Def* def, Defs defs);

bool is_unit(const Def*);

std::string tuple2str(const Def*);

} // namespace thorin
