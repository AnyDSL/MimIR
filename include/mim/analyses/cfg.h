#pragma once

#include "mim/analyses/scope.h"
#include "mim/util/indexmap.h"
#include "mim/util/indexset.h"

namespace mim {

class CFNode;
// clang-format off
class LoopTree;
class DomTree;
class DomFrontier;
// clang-format on

/// @name Control Flow
///@{
using CFNodes = GIDSet<const CFNode*>;
///@}

/// A Control-Flow Node.
/// Managed by CFA.
class CFNode {
public:
    CFNode(Def* mut)
        : mut_(mut)
        , gid_(gid_counter_++) {}

    uint64_t gid() const { return gid_; }
    size_t index() const { return index_; }
    Def* mut() const { return mut_; }
    const CFNodes& preds() const { return preds_; }
    const CFNodes& succs() const { return succs_; }
    size_t num_preds() const { return preds().size(); }
    size_t num_succs() const { return succs().size(); }

private:
    void link(const CFNode* other) const;

    mutable size_t index_ = -1; ///< RPO index in a CFG.

    Def* mut_;
    size_t gid_;
    static uint64_t gid_counter_;
    mutable CFNodes preds_;
    mutable CFNodes succs_;

    friend class CFA;
    friend class CFG;
};

/// @name std::ostream operator
///@{
std::ostream& operator<<(std::ostream&, const CFNode*);
///@}

//------------------------------------------------------------------------------

/// Control Flow Analysis.
class CFA {
public:
    CFA(const CFA&)     = delete;
    CFA& operator=(CFA) = delete;

    explicit CFA(const Scope& scope);
    ~CFA();

    const Scope& scope() const { return scope_; }
    World& world() const { return scope().world(); }
    size_t size() const { return nodes().size(); }
    const MutMap<const CFNode*>& nodes() const { return nodes_; }
    const CFG& cfg() const;
    const CFNode* operator[](Def* mut) const {
        auto i = nodes_.find(mut);
        return i == nodes_.end() ? nullptr : i->second;
    }

private:
    void verify();
    const CFNode* entry() const { return entry_; }
    const CFNode* node(Def*);

    const Scope& scope_;
    MutMap<const CFNode*> nodes_;
    const CFNode* entry_;
    mutable std::unique_ptr<const CFG> cfg_;

    friend class CFG;
};

//------------------------------------------------------------------------------

/// A Control-Flow Graph.
/// A small wrapper for the information obtained by a CFA.
/// The template parameter @p forward determines the direction of the edges:
/// * `true` means a conventional CFG.
/// * `false* means that all edges in this CFG are reversed.
/// Thus, a [dominance analysis](DomTreeBase), for example, becomes a post-dominance analysis.
class CFG {
public:
    template<class Value> using Map = IndexMap<CFG, const CFNode*, Value>;
    using Set                       = IndexSet<CFG, const CFNode*>;

    CFG(const CFG&)     = delete;
    CFG& operator=(CFG) = delete;

    explicit CFG(const CFA&);

    const CFA& cfa() const { return cfa_; }
    size_t size() const { return cfa().size(); }
    const CFNode* entry() const { return cfa().entry(); }
    auto reverse_post_order() const { return rpo_.array().view(); }
    auto post_order() const { return std::views::reverse(rpo_.array()); }
    /// Maps from reverse post-order index to CFNode.
    const CFNode* reverse_post_order(size_t i) const { return rpo_.array()[i]; }
    /// Maps from post-order index to CFNode.
    const CFNode* post_order(size_t i) const { return rpo_.array()[size() - 1 - i]; }
    const CFNode* operator[](Def* mut) const { return cfa()[mut]; } ///< Maps from @p mut to CFNode.
    const DomTree& domtree() const;
    const LoopTree& looptree() const;
    const DomFrontier& domfrontier() const;

    static size_t index(const CFNode* n) { return n->index_; }

private:
    size_t post_order_visit(const CFNode* n, size_t i);

    const CFA& cfa_;
    Map<const CFNode*> rpo_;
    mutable std::unique_ptr<const DomTree> domtree_;
    mutable std::unique_ptr<const LoopTree> looptree_;
    mutable std::unique_ptr<const DomFrontier> domfrontier_;
};

//------------------------------------------------------------------------------

} // namespace mim
