#pragma once

#include "thorin/def.h"

namespace thorin {

class RuleType : public Def {
private:
    RuleType(World&);

public:
    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    static constexpr auto Node = Node::RuleType;
    friend class World;
};

class Rule : public Def {
private:
    Rule(const Def* type, const Def* dbg)
        : Def(Node, type, 4, 0, dbg) {}

public:
    /// @name ops
    ///@{

    /// Signature of the ptrn variable.
    const Def* dom() const { return op(0); }
    const Def* ptrn() const { return op(1); }
    const Def* guard() const { return op(2); }
    const Def* rhs() const { return op(3); }
    ///@}

    /// @name ops
    ///@{
    Rule* set_dom(const Def* dom) { return Def::set(0, dom)->as<Rule>(); }
    Rule* set(const Def* ptrn, const Def* guard, const Def* rhs) {
        Def::set(1, ptrn);
        Def::set(2, guard);
        return Def::set(3, rhs)->as<Rule>();
    }
    ///@}

    /// @name type
    ///@{
    const RuleType* type() const { return Def::type()->as<RuleType>(); }
    ///@}

    /// @name virtual methods
    ///@{
    Rule* stub(World&, const Def*, const Def*) override;
    ///@}

    static constexpr auto Node = Node::Rule;
    friend class World;
};

} // namespace thorin
