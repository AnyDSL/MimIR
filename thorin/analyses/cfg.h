#pragma once

#include <vector>

#include "thorin/analyses/scope.h"
#include "thorin/util/indexmap.h"
#include "thorin/util/indexset.h"
#include "thorin/util/print.h"
#include "thorin/util/span.h"

namespace thorin {

class CFNode;
// clang-format off
template<bool> class LoopTree;
template<bool> class DomTreeBase;
template<bool> class DomFrontierBase;
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
    Def* mut() const { return mut_; }

private:
    const CFNodes& preds() const { return preds_; }
    const CFNodes& succs() const { return succs_; }
    void link(const CFNode* other) const;

    mutable size_t f_index_ = -1; ///< RPO index in a **forward** CFG.
    mutable size_t b_index_ = -1; ///< RPO index in a **backwards** CFG.

    Def* mut_;
    size_t gid_;
    static uint64_t gid_counter_;
    mutable CFNodes preds_;
    mutable CFNodes succs_;

    friend class CFA;
    template<bool> friend class CFG;
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
    const F_CFG& f_cfg() const;
    const B_CFG& b_cfg() const;
    const CFNode* operator[](Def* mut) const {
        auto i = nodes_.find(mut);
        return i == nodes_.end() ? nullptr : i->second;
    }

private:
    void link_to_exit();
    void verify();
    const CFNodes& preds(Def* mut) const {
        auto cn = nodes_.find(mut)->second;
        assert(cn);
        return cn->preds();
    }
    const CFNodes& succs(Def* mut) const {
        auto cn = nodes_.find(mut)->second;
        assert(cn);
        return cn->succs();
    }
    const CFNode* entry() const { return entry_; }
    const CFNode* exit() const { return exit_; }
    const CFNode* node(Def*);

    const Scope& scope_;
    MutMap<const CFNode*> nodes_;
    const CFNode* entry_;
    const CFNode* exit_;
    mutable std::unique_ptr<const F_CFG> f_cfg_;
    mutable std::unique_ptr<const B_CFG> b_cfg_;

    template<bool> friend class CFG;
};

//------------------------------------------------------------------------------

/// A Control-Flow Graph.
/// A small wrapper for the information obtained by a CFA.
/// The template parameter @p forward determines the direction of the edges:
/// * `true` means a conventional CFG.
/// * `false* means that all edges in this CFG are reversed.
/// Thus, a [dominance analysis](DomTreeBase), for example, becomes a post-dominance analysis.
template<bool forward> class CFG {
public:
    template<class Value> using Map = IndexMap<CFG<forward>, const CFNode*, Value>;
    using Set                       = IndexSet<CFG<forward>, const CFNode*>;

    CFG(const CFG&)     = delete;
    CFG& operator=(CFG) = delete;

    explicit CFG(const CFA&);

    const CFA& cfa() const { return cfa_; }
    size_t size() const { return cfa().size(); }
    const CFNodes& preds(const CFNode* n) const;
    const CFNodes& succs(const CFNode* n) const;
    const CFNodes& preds(Def* mut) const { return preds(cfa()[mut]); }
    const CFNodes& succs(Def* mut) const { return succs(cfa()[mut]); }
    size_t num_preds(const CFNode* n) const { return preds(n).size(); }
    size_t num_succs(const CFNode* n) const { return succs(n).size(); }
    size_t num_preds(Def* mut) const { return num_preds(cfa()[mut]); }
    size_t num_succs(Def* mut) const { return num_succs(cfa()[mut]); }
    const CFNode* entry() const { return forward ? cfa().entry() : cfa().exit(); }
    const CFNode* exit() const { return forward ? cfa().exit() : cfa().entry(); }

    auto reverse_post_order() const { return rpo_.array().view(); }
    auto post_order() const { return std::views::reverse(rpo_.array()); }
    /// Maps from reverse post-order index to CFNode.
    const CFNode* reverse_post_order(size_t i) const { return rpo_.array()[i]; }
    /// Maps from post-order index to CFNode.
    const CFNode* post_order(size_t i) const { return rpo_.array()[size() - 1 - i]; }
    const CFNode* operator[](Def* mut) const { return cfa()[mut]; } ///< Maps from @p mut to CFNode.
    const DomTreeBase<forward>& domtree() const;
    const LoopTree<forward>& looptree() const;
    const DomFrontierBase<forward>& domfrontier() const;

    static size_t index(const CFNode* n) { return forward ? n->f_index_ : n->b_index_; }

private:
    size_t post_order_visit(const CFNode* n, size_t i);

    const CFA& cfa_;
    Map<const CFNode*> rpo_;
    mutable std::unique_ptr<const DomTreeBase<forward>> domtree_;
    mutable std::unique_ptr<const LoopTree<forward>> looptree_;
    mutable std::unique_ptr<const DomFrontierBase<forward>> domfrontier_;
};

//------------------------------------------------------------------------------

} // namespace thorin
