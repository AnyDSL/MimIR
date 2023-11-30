#include "thorin/be/dot/dot.h"

#include "thorin/def.h"
#include "thorin/world.h"

#include "thorin/be/emitter.h"
#include "thorin/util/print.h"

namespace thorin::dot {

namespace {

void emit_cluster_start(std::ostream& os, Lam* lam) {
    print(os, "subgraph cluster_{} {{\n", lam->unique_name());
    print(os, "shape=square;\n");
    print(os, "label=\"{}\";\n", lam->unique_name());
    print(os, "\"{}:{}\" [shape=rect];\n", lam->node_name(), lam->unique_name());
    if (auto body = lam->body())
        print(os, "\"{}:{}\"->\"{}:{}\" [style=dashed];\n", lam->node_name(), lam->unique_name(), body->node_name(),
              body->unique_name());
}

void emit_node_attributes(std::ostream& stream, const Def* def) {
    if (def->isa<Var>()) stream << ", color=blue";
}

} // namespace

class BB {};

class Emitter : public thorin::Emitter<std::string, std::string, BB, Emitter> {
public:
    using Super = thorin::Emitter<std::string, std::string, BB, Emitter>;

    Emitter(World& w, std::ostream& ostream, const std::function<void(std::ostream&, const Def*)>& stream_def)
        : Super(w, "dot_emitter", ostream)
        , stream_def_(stream_def) {
        ostream << "digraph Thorin {\n";
    }

    ~Emitter() {
        ostream_ << connections_.str();
        ostream_ << "}\n";
    }

    bool is_valid(std::string_view s) { return !s.empty(); }
    void emit_imported(Lam*);
    void emit_epilogue(Lam*);

    std::string emit_bb(BB&, const Def*);
    std::string prepare(const Scope&);
    void finalize(const Scope&);

private:
    std::function<void(std::ostream&, const Def*)> stream_def_;
    DefSet visited_muts_;
    std::ostringstream connections_;
};

void default_stream_def(std::ostream& s, const Def* def) { def->stream(s, 0); }

void emit(World& w, std::ostream& s, std::function<void(std::ostream&, const Def*)> stream_def) {
    Emitter emitter{w, s, stream_def};
    emitter.run();
}

void Emitter::emit_imported(Lam* lam) {
    print(ostream_, "\"{}:{}\" [shape=rect];\n", lam->node_name(), lam->unique_name());
}

void Emitter::emit_epilogue(Lam* lam) {
    if (visited_muts_.contains(lam)) return;
    visited_muts_.insert(lam);

    if (lam != entry_) {
        emit_cluster_start(ostream_, lam);
        emit(lam->body());
        print(ostream_, "}}\n");
    } else {
        emit(lam->body());
    }
}

std::string Emitter::emit_bb(BB&, const Def* def) {
    if (auto lam = def->isa<Lam>()) return lam->sym().str();

    print(ostream_, "\"{}:{}\" [label=\"", def->node_name(), def->unique_name());
    stream_def_(ostream_, def);
    ostream_ << "\"";
    emit_node_attributes(ostream_, def);
    ostream_ << "];\n";

    for (auto op : def->ops()) {
        emit_unsafe(op);
        print(connections_, "\"{}:{}\"->\"{}:{}\";\n", def->node_name(), def->unique_name(), op->node_name(),
              op->unique_name());
    }

    return {def->unique_name()};
}

std::string Emitter::prepare(const Scope& scope) {
    auto lam = scope.entry()->as_mut<Lam>();

    emit_cluster_start(ostream_, lam);

    return lam->sym().str();
}

void Emitter::finalize(const Scope&) { ostream_ << "}\n"; }

} // namespace thorin::dot
