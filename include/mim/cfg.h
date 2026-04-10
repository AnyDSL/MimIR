#include <memory>

#include "mim/def.h"
#include "mim/lam.h"
#include "mim/tuple.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
namespace mim {

class CFG {
public:
    class Node {
    public:
        const Lam* mut() const { return mut_; }
        const auto& succs() const { return succs_; }
        const auto& preds() const { return preds_; }
        /// [Immediate Dominator](https://en.wikipedia.org/wiki/Dominator_(graph_theory)) of this Node, or itself
        /// if it is the entry. Computed eagerly when the CFG is constructed.
        const Node* idom() const { return idom_; }
        /// Nodes higher up in the dominator tree have higher postorder numbers.
        /// Used to efficiently find the LCA in the dominator tree via CFG::lca.
        size_t postorder_number() const { return postorder_number_; }

    private:
        Node(CFG& cfg, const Lam* mut)
            : cfg_(cfg) {
            mut_ = mut;
        }

        void follow_escaping(const App* app);
        bool add_edge(const Lam* succ);
        void init();

        CFG& cfg_;
        const Lam* mut_;
        const Node* idom_        = nullptr;
        size_t postorder_number_ = 0;

        absl::flat_hash_set<Node*> succs_;
        absl::flat_hash_set<Node*> preds_;

        friend class CFG;
    };

    class Loop {
    public:
    private:
        absl::flat_hash_set<Node*> nodes_;
        absl::flat_hash_set<Node*> exits_;
    };

    CFG(Lam* entry, bool include_closed = false);

    void cfa();

    const Node* entry() const { return entry_; }

    /// @name Dominance
    ///@{
    /// Least common ancestor of @p n and @p m in the dominator tree.
    static const Node* lca(const Node* n, const Node* m);
    ///@}

    /// @name Nodes
    ///@{
    size_t num_nodes() const { return mut2node_.size(); }
    // clang-format off
    auto muts()  const { return mut2node_ | std::views::keys; }
    auto nodes() const { return mut2node_ | std::views::transform([](const auto& p) { return (const Node*)p.second.get(); }); }
    // clang-format on
    const Node* operator[](const Def* def) const { return (def->isa<Lam>() ? (*this)[def->as<Lam>()] : nullptr); }
    ///@}

    /// @name dot
    /// GraphViz output.
    ///@{
    void dot(std::ostream& os) const;
    void dot(const char* file = nullptr) const;
    void dot(std::string s) const { dot(s.c_str()); }
    ///@}

private:
    Node* visit(const Lam* mut);

    void assign_postorder_numbers();
    void calc_dominance();

    Node* operator[](const Lam* mut) {
        if (auto i = mut2node_.find(mut); i != mut2node_.end()) return i->second.get();
        return nullptr;
    }

    World& world_;
    absl::flat_hash_map<const Lam*, std::unique_ptr<Node>> mut2node_;
    Node* entry_;
    bool include_closed_;
};

} // namespace mim
