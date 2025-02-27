#include <fstream>
#include <memory>
#include <set>
#include <stack>
#include <vector>

#include <gtest/gtest.h>

#include "mim/util/print.h"
#include "mim/util/util.h"

namespace mim {

struct Graph {
    struct Node {
        Node(int data)
            : data(data) {}

        Node* link(Node* next) {
            this->succs.emplace(next);
            return this;
        }

        void dot(std::ostream& os) {
            for (auto succ : succs) println(os, "{} -> {}", data, succ->data);
        }

        struct Lt {
            bool operator()(Node* n, Node* m) const { return n->data < m->data; }
        };

        int data;
        std::set<Node*, Lt> succs;
        struct {
            unsigned idx      : 31 = 0;
            unsigned visited  : 1  = false;
            unsigned low      : 30 = 0;
            unsigned on_stack : 1  = false;
            unsigned rec      : 1  = true;
        } mutable impl;
    };

    void dot(std::ostream& os) {
        println(os, "digraph {{");
        println(os, "ordering=out;");
        println(os, "splines=false;");
        println(os, "node [shape=box,style=filled];");
        for (const auto& node : nodes) {
            println(os, "{} [label=\"{} {} {}{}\"]", node->data, node->data, (unsigned)node->impl.idx,
                    (unsigned)node->impl.low, node->impl.rec ? " rec" : "");
        }
        for (const auto& node : nodes) node->dot(os);
        println(os, "}}");
    }

    void dot(const char* name) {
        std::ofstream of(name);
        dot(of);
    }

    Node* node(int data) {
        nodes.emplace_back(std::make_unique<Node>(data));
        return nodes.back().get();
    }

    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<Node*> topo;
};

int dfs(int i, Graph& g, Graph::Node* curr, std::stack<Graph::Node*>& stack) {
    curr->impl.idx = curr->impl.low = i++;
    curr->impl.visited              = true;
    curr->impl.on_stack             = true;
    stack.emplace(curr);

    for (auto succ : curr->succs) {
        if (!succ->impl.visited) i = dfs(i, g, succ, stack);
        if (succ->impl.on_stack) curr->impl.low = std::min(curr->impl.low, succ->impl.low);
    }

    if (curr->impl.idx == curr->impl.low) {
        Graph::Node* node;
        int num = 0;
        do {
            node = pop(stack);
            ++num;
            node->impl.on_stack = false;
            node->impl.low      = curr->impl.idx;
        } while (node != curr);

        if (num == 1 && !curr->succs.contains(curr)) curr->impl.rec = false;
        g.topo.emplace_back(curr);
    }

    return i;
}

void tarjan(Graph& graph) {
    std::stack<Graph::Node*> stack;
    for (int i = 0; const auto& node : graph.nodes)
        if (!node->impl.visited) i = dfs(i, graph, node.get(), stack);
}

// https://www.youtube.com/watch?v=wUgWX0nc4NY
TEST(Tarjan, test1) {
    Graph g;
    auto n0 = g.node(0), n1 = g.node(1), n2 = g.node(2), n3 = g.node(3), n4 = g.node(4), n5 = g.node(5), n6 = g.node(6),
         n7 = g.node(7);

    n0->link(n1);
    n1->link(n2);
    n2->link(n0);
    n3->link(n4)->link(n7);
    n4->link(n5);
    n5->link(n6)->link(n0);
    n6->link(n0)->link(n2)->link(n4);
    n7->link(n3)->link(n5);

    tarjan(g);
    g.dot("test1.dot");

    ASSERT_EQ(n0->impl.low, 0);
    ASSERT_EQ(n1->impl.low, 0);
    ASSERT_EQ(n2->impl.low, 0);

    ASSERT_EQ(n4->impl.low, 4);
    ASSERT_EQ(n5->impl.low, 4);
    ASSERT_EQ(n6->impl.low, 4);

    ASSERT_EQ(n3->impl.low, 3);
    ASSERT_EQ(n7->impl.low, 3);
}

// https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm#/media/File:Tarjan's_Algorithm_Animation.gif
TEST(Tarjan, test2) {
    Graph g;
    auto n1 = g.node(1), n2 = g.node(2), n3 = g.node(3), n4 = g.node(4), n5 = g.node(5), n6 = g.node(6), n7 = g.node(7),
         n8 = g.node(8);

    n1->link(n2);
    n2->link(n3);
    n3->link(n1);
    n4->link(n2)->link(n5);
    n5->link(n4)->link(n6);
    n6->link(n3)->link(n7);
    n7->link(n6);
    n8->link(n5)->link(n7)->link(n8);

    tarjan(g);
    g.dot("test2.dot");

    ASSERT_EQ(n1->impl.low, 0);
    ASSERT_EQ(n2->impl.low, 0);
    ASSERT_EQ(n3->impl.low, 0);

    ASSERT_EQ(n4->impl.low, 3);
    ASSERT_EQ(n5->impl.low, 3);

    ASSERT_EQ(n6->impl.low, 5);
    ASSERT_EQ(n7->impl.low, 5);

    ASSERT_EQ(n8->impl.low, 7);
}

TEST(Tarjan, test3) {
    Graph g;
    auto n0 = g.node(0), n1 = g.node(1), n2 = g.node(2), n3 = g.node(3);

    n0->link(n1);
    n1->link(n2)->link(n3);
    n2->link(n1);

    tarjan(g);
    g.dot("test3.dot");
}

} // namespace mim
