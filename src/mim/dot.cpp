#include <fstream>
#include <ostream>
#include <sstream>

#include "mim/def.h"
#include "mim/world.h"

#include "mim/analyses/nest.h"
#include "mim/util/print.h"

// Do not zonk here!
// We want to see all Refs in the DOT graph.

namespace mim {

namespace {

template<class T> std::string escape(const T& val) {
    std::ostringstream oss;
    oss << val;
    auto str = oss.str();
    find_and_replace(str, "<", "&lt;");
    find_and_replace(str, ">", "&gt;");
    return str;
}

class Dot {
public:
    Dot(std::ostream& ostream, bool types, const Def* root = nullptr)
        : os_(ostream)
        , types_(types)
        , root_(root) {}

    void prologue() {
        (tab_++).println(os_, "digraph {{");
        tab_.println(os_, "ordering=out;");
        tab_.println(os_, "splines=false;");
        tab_.println(os_, "node [shape=box,style=filled];");
    }

    void epilogue() { (--tab_).println(os_, "}}"); }

    void run(const Def* root, uint32_t max) {
        prologue();
        recurse(root, max);
        epilogue();
    }

    void recurse(const Def* def, uint32_t max) {
        if (max == 0 || !done_.emplace(def).second) return;
        tab_.print(os_, "_{}[", def->gid());
        if (def->isa_mut())
            if (def == root_)
                os_ << "style=\"filled,diagonals,bold\"";
            else
                os_ << "style=\"filled,diagonals\"";
        else if (def == root_)
            os_ << "style=\"filled,bold\"";
        label(def) << ',';
        color(def) << ',';
        if (def->free_vars().empty()) os_ << "rank=min,";
        tooltip(def) << "];\n";

        if (!def->is_set()) return;

        for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
            auto op = def->op(i).def();
            recurse(op, max - 1);
            tab_.print(os_, "_{} -> _{}[taillabel=\"{}\",", def->gid(), op->gid(), i);
            if (op->isa<Lit>() || op->isa<Axiom>() || def->isa<Var>() || def->isa<Nat>() || def->isa<Idx>())
                print(os_, "fontcolor=\"#00000000\",color=\"#00000000\",constraint=false];\n");
            else
                print(os_, "];\n");
        }

        if (auto t = def->type().def(); t && types_) {
            recurse(t, max - 1);
            tab_.println(os_, "_{} -> _{}[color=\"#00000000\",constraint=false,style=dashed];", def->gid(), t->gid());
        }
    }

    std::ostream& label(const Def* def) {
        print(os_, "label=<{}<br/>", def->unique_name());
        if (auto lit = def->isa<Lit>())
            lit->stream(os_, 0);
        else
            os_ << def->node_name();
        return os_ << '>';
    }

    std::ostream& color(const Def* def) {
        return print(os_, "fillcolor=\"{} 0.5 0.75\"", def->node() / (float)Node::Num_Nodes);
    }

    std::ostream& tooltip(const Def* def) {
        static constexpr auto NL = "&#13;&#10;";
        auto loc                 = escape(def->loc());
        auto type                = escape(def->type().def());
        escape(loc);
        print(os_, "tooltip=\"");
        print(os_, "<b>expr:</b> {}{}", def, NL);
        print(os_, "<b>type:</b> {}{}", type, NL);
        print(os_, "<b>name:</b> {}{}", def->sym(), NL);
        print(os_, "<b>gid:</b> {}{}", def->gid(), NL);
        print(os_, "<b>flags:</b> 0x{x}{}", def->flags(), NL);
        print(os_, "<b>free_vars:</b> {{{, }}}{}", def->free_vars(), NL);
        print(os_, "<b>local_vars:</b> {{{, }}}{}", def->local_vars(), NL);
        print(os_, "<b>local_muts:</b> {{{, }}}{}", def->local_muts(), NL);
        if (auto mut = def->isa_mut()) print(os_, "<b>fv_consumers:</b> {{{, }}}{}", mut->fv_consumers(), NL);
        print(os_, "<b>loc:</b> {}", loc);
        return print(os_, "\"");
    }

private:
    std::ostream& os_;
    bool types_;
    const Def* root_;
    Tab tab_;
    DefSet done_;
};

} // namespace

void Def::dot(std::ostream& ostream, uint32_t max, bool types) const { Dot(ostream, types, this).run(this, max); }

void Def::dot(const char* file, uint32_t max, bool types) const {
    if (!file) {
        dot(std::cout, max, types);
    } else {
        auto of = std::ofstream(file);
        dot(of, max, types);
    }
}

void World::dot(const char* file, bool annexes, bool types) const {
    if (!file) {
        dot(std::cout, annexes, types);
    } else {
        auto of = std::ofstream(file);
        dot(of, annexes, types);
    }
}

void World::dot(std::ostream& os, bool anx, bool types) const {
    Dot dot(os, types);
    dot.prologue();
    for (auto [_, external] : externals()) dot.recurse(external, uint32_t(-1));
    if (anx)
        for (auto [_, annex] : annexes()) dot.recurse(annex, uint32_t(-1));
    dot.epilogue();
}

/*
 * Nest
 */

void Nest::dot(const char* file) const {
    if (!file) {
        dot(std::cout);
    } else {
        auto of = std::ofstream(file);
        dot(of);
    }
}

void Nest::dot(std::ostream& os) const {
    Tab tab;
    (tab++).println(os, "digraph {{");
    tab.println(os, "ordering=out;");
    tab.println(os, "splines=false;");
    tab.println(os, "node [shape=box,style=filled];");
    root()->dot(tab, os);
    (--tab).println(os, "}}");
}

void Nest::Node::dot(Tab tab, std::ostream& os) const {
    for (const auto& [child, _] : children()) tab.println(os, "{} -> {}", mut()->unique_name(), child->unique_name());
    for (const auto& [_, child] : children()) child->dot(tab, os);
}

} // namespace mim
