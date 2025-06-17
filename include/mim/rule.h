#pragma once

#include "mim/def.h"

namespace mim {

/// A rewrite rule type
/// currently opaque
class RuleType : public Def, public Setters<RuleType> {
protected:
    RuleType(const Def* type, Defs meta_types)
        : Def(Node, type, meta_types, 0) {}
    RuleType(const Def* type, size_t size)
        : Def(Node, type, size, 0) {} ///< Constructor for a *mutable* RuleType

public:
    Defs meta_types() { return ops(); }

    /// @name Setters
    ///@{
    using Setters<RuleType>::set;
    RuleType* set(size_t i, const Def* def) { return Def::set(i, def)->as<RuleType>(); }
    RuleType* set(Defs ops) { return Def::set(ops)->as<RuleType>(); }
    RuleType* unset() { return Def::unset()->as<RuleType>(); }
    ///@}

    ///@name Rebuild
    ///@{
    const RuleType* immutabilize() override;
    ///@}

    static const Def* infer(const Defs meta_type);
    const Def* check() override;

    static constexpr auto Node = mim::Node::RuleType;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// A rewrite rule
// TODO : variables ?????
class Rule : public Def, public Setters<Rule> {
private:
    Rule(const RuleType* type, const Def* lhs, const Def* rhs)
        : Def(Node, type, {lhs, rhs}, 0) {}
    Rule(const RuleType* type)
        : Def(Node, type, 2, 0) {}

public:
    /// @name lhs & rhs
    /// @anchor
    /// @see @ref proj
    ///@{
    const RuleType* type() const { return Def::type()->as<RuleType>(); }
    const Def* lhs() const { return op(0); }
    const Def* rhs() const { return op(1); }
    MIM_PROJ(lhs, const)
    MIM_PROJ(rhs, const)
    ///@}

    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Rule>::set;
    Rule* set(const Def* lhs, const Def* rhs) { return set_lhs(lhs)->set_rhs(rhs); }
    Rule* set_lhs(const Def* lhs) { return Def::set(0, lhs)->as<Rule>(); }
    Rule* set_rhs(const Def* rhs) { return Def::set(1, rhs)->as<Rule>(); }
    Rule* unset() { return Def::unset()->as<Rule>(); }
    ///@}

    /// @name Type checking
    ///@{
    const Def* check(size_t, const Def*) override;
    const Def* check() override;
    //    static const Def* infer(World&, const Def*, const Def*);
    ///@}

    /// @name Rebuild
    ///@{
    Rule* stub(const Def* type) { return stub_(world(), type)->set(dbg()); }
    const Rule* immutabilize() override;
    const Def* reduce(const Def* arg) const { return Def::reduce(arg).front(); }
    constexpr size_t reduction_offset() const noexcept override { return 1; }
    ///@}

    static constexpr auto Node = mim::Node::Rule;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;
    Rule* stub_(World&, const Def*) override;

    friend class World;
};
} // namespace mim
