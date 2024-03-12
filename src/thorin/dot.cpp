#include <fstream>
#include <ostream>

#include "thorin/def.h"
#include "thorin/world.h"

#include "thorin/util/print.h"

namespace thorin {

namespace {

template<class T> std::string to_string(const T& val) {
    std::ostringstream oss;
    oss << val;
    return oss.str();
}

void escape(std::string& str) {
    for (auto i = str.find('<'); i != std::string::npos; i = str.find('<', i)) str.replace(i, 1, "&lt;");
    for (auto i = str.find('>'); i != std::string::npos; i = str.find('>', i)) str.replace(i, 1, "&gt;");
}

} // namespace

class Dot {
public:
    Dot(std::ostream& ostream)
        : os_(ostream) {}

    void prologue() {
        tab_.println(os_, "digraph {{");
        ++tab_;
        tab_.println(os_, "ordering=out;");
        tab_.println(os_, "node [shape=box];");
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
        if (max <= 0 || !done_.emplace(def).second) return;
        // auto l = label(def);
        auto style = def->isa_mut() ? ",style=bold" : "";
        tab_.print(os_, "_{}[label=<{}<br/>{}>{}", def->gid(), def->unique_name(), def->node_name(), style);
        tooltip(def);
        println(os_, "];");

        if (def->is_set()) {
            for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
                auto op = def->op(i);
                recurse(op, max - 1);
                if (op->isa<Lit>() || op->isa<Axiom>() || def->isa<Var>())
                    tab_.println(os_, "_{} -> _{}[color=\"#00000000\",constraint=false];", def->gid(), op->gid());
                else
                    tab_.println(os_, "_{} -> _{}[label=\"{}\"];", def->gid(), op->gid(), i);
            }
#if 0
            if (auto type = def->type()) {
                recurse(type, max - 1);
                tab_.println(os_, "_{} -> _{}[color=\"#00000000\",constraint=false,style=dashed];", def->gid(), type->gid());
            }
#endif
        }
    }

    void tooltip(const Def* def) {
        static constexpr auto NL = "&#13;&#10;";
        auto loc                 = to_string(def->loc());
        std::ostringstream oss;
        oss << std::hex << def->flags();
        auto flags = oss.str();
        escape(loc);
        print(os_, ",tooltip=\"") << def << NL;
        print(os_, "<b>name:</b> {}{}", def->sym(), NL);
        print(os_, "<b>gid:</b> {}{}", def->gid(), NL);
        print(os_, "<b>flags:</b> {}{}", flags, NL);
        print(os_, "<b>name:</b> {}{}", def->sym(), NL);
        print(os_, "<b>loc:</b> {}{}", loc, NL);
        print(os_, "\"");
    }

    std::string label(const Def* def) const {
        if (auto lit = Lit::isa(def)) return std::to_string(*lit);
        switch (def->node()) {
            case Node::Axiom: return (std::string)def->sym();
            case Node::Pi: return "Π";
            case Node::Lam: return "λ";
            case Node::App: return "";
            case Node::Sigma: return "Σ";
            case Node::Tuple: return "()";
            case Node::Extract: return "#";
            default: return std::string(def->node_name());
        }
    }

private:
    std::ostream& os_;
    Tab tab_;
    DefSet done_;
};

void Def::dot(uint32_t max, std::ostream& ostream) const { Dot(ostream).run(this, max); }

void Def::dot(uint32_t max) const { dot(max, std::cout); }

void Def::dot(uint32_t max, const char* file) const {
    if (!file) {
        dot(max, std::cout);
    } else {
        auto of = std::ofstream(file);
        dot(max, of);
    }
}

void World::dot(std::ostream& os) const {
    Dot dot(os);
    dot.prologue();
    for (auto [_, external] : externals()) dot.recurse(external, uint32_t(-1));
    dot.epilogue();
}

} // namespace thorin
