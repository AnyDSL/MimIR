#pragma once

#include "thorin/def.h"

namespace thorin {

class RuleType : public Def {
private:
    RuleType(const Def* type, const Def* dom, const Def* dbg)
        : Def(Node, type, {dom}, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* dom() const { return op(0); }
    ///@}

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
        : Def(Node, type, 2, 0, dbg) {}

public:
    /// @name ops
    ///@{
    const Def* lhs() const { return op(0); }
    const Def* rhs() const { return op(1); }
    ///@}

    /// @name ops
    ///@{
    Rule* set(const Def* lhs, const Def* rhs) {
        Def::set(0, lhs);
        return Def::set(1, rhs)->as<Rule>();
    }
    ///@}

    /// @name type
    ///@{
    const RuleType* type() const { return Def::type()->as<RuleType>(); }
    const Def* dom() const { return type()->dom(); }
    ///@}

    /// @name virtual methods
    ///@{
    Rule* stub(World&, const Def*, const Def*) override;
    ///@}

    static constexpr auto Node = Node::Rule;
    friend class World;
};

} // namespace thorin
