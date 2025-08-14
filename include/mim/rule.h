#pragma once

#include "mim/def.h"

namespace mim {

/// Type <b>form</b>ation of a <b>re</b>write Rule.
/// Currently opaque.
class Reform : public Def, public Setters<Reform> {
protected:
    Reform(const Def* type, const Def* meta_type)
        : Def(Node, type, {meta_type}, 0) {}

public:
    const Def* meta_type() const { return op(0); }

    /// @name Setters
    ///@{
    using Setters<Reform>::set;
    Reform* set(size_t i, const Def* def) { return Def::set(i, def)->as<Reform>(); }
    Reform* set(Defs ops) { return Def::set(ops)->as<Reform>(); }
    Reform* unset() { return Def::unset()->as<Reform>(); }
    ///@}

    static const Def* infer(const Def* meta_type);
    const Def* check() override;

    static constexpr auto Node      = mim::Node::Reform;
    static constexpr size_t Num_Ops = 1;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;

    friend class World;
};

/// A rewrite rule
class Rule : public Def, public Setters<Rule> {
private:
    Rule(const Reform* type, const Def* lhs, const Def* rhs, const Def* condition)
        : Def(Node, type, {lhs, rhs, condition}, 0) {}
    Rule(const Reform* type)
        : Def(Node, type, 3, 0) {}

public:
    /// @name lhs & rhs
    /// @see @ref proj
    ///@{
    const Reform* type() const { return Def::type()->as<Reform>(); }
    const Def* lhs() const { return op(0); }
    const Def* rhs() const { return op(1); }
    const Def* condition() const { return op(2); }
    MIM_PROJ(lhs, const)
    MIM_PROJ(rhs, const)
    MIM_PROJ(condition, const)
    ///@}

    /// @name Setters
    /// @see @ref set_ops "Setting Ops"
    ///@{
    using Setters<Rule>::set;
    Rule* set(const Def* lhs, const Def* rhs) { return set_lhs(lhs)->set_rhs(rhs); }
    Rule* set(const Def* lhs, const Def* rhs, const Def* condition) {
        return set_lhs(lhs)->set_rhs(rhs)->set_condition(condition);
    }
    Rule* set_lhs(const Def* lhs) { return Def::set(0, lhs)->as<Rule>(); }
    Rule* set_rhs(const Def* rhs) { return Def::set(1, rhs)->as<Rule>(); }
    Rule* set_condition(const Def* condition) { return Def::set(2, condition)->as<Rule>(); }
    Rule* unset() { return Def::unset()->as<Rule>(); }
    ///@}

    /// @name Type checking
    ///@{
    const Def* check(size_t, const Def*) override;
    const Def* check() override;
    ///@}

    /// @name Rebuild
    ///@{
    Rule* stub(const Def* type) { return stub_(world(), type)->set(dbg()); }
    const Rule* immutabilize() override;
    const Def* reduce(const Def* arg) const { return Def::reduce(arg).front(); }
    constexpr size_t reduction_offset() const noexcept override { return 1; }
    ///@}

    /// @name Apply
    ///@{
    bool its_a_match(const Def* expr, Def2Def&) const;
    const Def* replace(const Def* expr, Def2Def&) const;
    ///@}

    static bool is_in_rule(const Def*);

    static constexpr auto Node      = mim::Node::Rule;
    static constexpr size_t Num_Ops = 3;

private:
    const Def* rebuild_(World&, const Def*, Defs) const override;
    Rule* stub_(World&, const Def*) override;
    bool its_a_match_(const Def* lhs, const Def* rhs, Def2Def& seen) const;
    friend class World;
};
} // namespace mim
