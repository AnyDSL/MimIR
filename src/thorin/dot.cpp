#include <fstream>
#include <ostream>
#include <sstream>

#include "thorin/def.h"
#include "thorin/world.h"

#include "thorin/util/print.h"

namespace thorin {

namespace {

template<class T> std::string escape(const T& val) {
    std::ostringstream oss;
    oss << val;
    auto str = oss.str();
    for (auto i = str.find('<'); i != std::string::npos; i = str.find('<', i)) str.replace(i, 1, "&lt;");
    for (auto i = str.find('>'); i != std::string::npos; i = str.find('>', i)) str.replace(i, 1, "&gt;");
    return str;
}

class Dot {
public:
    Dot(std::ostream& ostream, bool types)
        : os_(ostream)
        , types_(types) {}

    void prologue() {
        tab_.println(os_, "digraph {{");
        ++tab_;
        tab_.println(os_, "ordering=out;");
        tab_.println(os_, "splines=false;");
        tab_.println(os_, "node [shape=box,style=filled];");
    }

    void epilogue() {
        --tab_;
        tab_.println(os_, "}}");
    }

    void run(const Def* root, uint32_t max) {
        prologue();
        recurse(root, max);
        epilogue();
    }

    void recurse(const Def* def, uint32_t max) {
        if (max == 0 || !done_.emplace(def).second) return;
        tab_.print(os_, "_{}[", def->gid());
        if (def->isa_mut()) os_ << "style=\"filled,diagonals\"";
        label(def) << ',';
        color(def) << ',';
        tooltip(def) << "];\n";

        if (!def->is_set()) return;

        for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
            auto op = def->op(i);
            recurse(op, max - 1);
            tab_.print(os_, "_{} -> _{}[", def->gid(), op->gid());
            if (op->isa<Lit>() || op->isa<Axiom>() || def->isa<Var>() || def->isa<Nat>() || def->isa<Idx>())
                print(os_, "taillabel=\"{}\",fontcolor=\"#00000000\",color=\"#00000000\",constraint=false];\n", i);
            else
                print(os_, "label=\"{}\"];\n", i);
        }

        if (auto t = def->type(); t && types_) {
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
        auto type                = escape(def->type());
        std::ostringstream oss;
        oss << std::hex << def->flags();
        auto flags = oss.str();
        escape(loc);
        print(os_, "tooltip=\"");
        print(os_, "<b>expr:</b> {}{}", def, NL);
        print(os_, "<b>type:</b> {}{}", type, NL);
        print(os_, "<b>name:</b> {}{}", def->sym(), NL);
        print(os_, "<b>gid:</b> {}{}", def->gid(), NL);
        print(os_, "<b>flags:</b> {}{}", flags, NL);
        print(os_, "<b>loc:</b> {}", loc);
        return print(os_, "\"");
    }

private:
    std::ostream& os_;
    bool types_;
    Tab tab_;
    DefSet done_;
};

} // namespace

void Def::dot(std::ostream& ostream, uint32_t max, bool types) const { Dot(ostream, types).run(this, max); }

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

} // namespace thorin
