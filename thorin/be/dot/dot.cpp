#include "thorin/be/dot/dot.h"

#include "thorin/def.h"
#include "thorin/world.h"

#include "thorin/be/emitter.h"
#include "thorin/util/stream.h"

namespace thorin::dot {

class BB {};

class DotEmitter : public Emitter<std::string, std::string, BB, DotEmitter> {
public:
    DotEmitter(World& w, Stream& s, const std::function<void(Stream&, const Def*)>& stream_def)
        : Emitter(w, s)
        , stream_def_(stream_def) {
        s << "digraph Thorin {\n";
    }

    ~DotEmitter() {
        stream_ << connections_.str();
        stream_ << "}\n";
    }

    bool is_valid(std::string_view s) { return !s.empty(); }
    void run() { emit_module(); }
    void emit_imported(Lam*);
    void emit_epilogue(Lam*);

    std::string emit_bb(BB&, const Def*);
    std::string prepare(const Scope&);
    void finalize(const Scope&);

private:
    std::function<void(Stream&, const Def*)> stream_def_;
    DefSet visited_noms_;
    StringStream connections_;
};

void default_stream_def(Stream& s, const Def* def) { def->stream(s, 0); }

void emit(World& w, Stream& s, std::function<void(Stream&, const Def*)> stream_def) {
    DotEmitter emitter{w, s, stream_def};
    emitter.run();
}

static void emit_cluster_start(Stream& stream, Lam* lam) {
    stream.fmt("subgraph cluster_{} {{\n", lam->unique_name());
    stream.fmt("shape=square;\n");
    stream.fmt("label=\"{}\";\n", lam->unique_name());
    stream.fmt("\"{}:{}\" [shape=rect];\n", lam->node_name(), lam->unique_name());
}

void DotEmitter::emit_imported(Lam* lam) {
    stream_.fmt("\"{}:{}\" [shape=rect];\n", lam->node_name(), lam->unique_name());
}
void DotEmitter::emit_epilogue(Lam* lam) {
    if (visited_noms_.contains(lam)) return;
    visited_noms_.insert(lam);

    auto body = lam->body()->as<App>();

    if (lam != entry_) {
        emit_cluster_start(stream_, lam);
        emit(lam->body());
        stream_.fmt("}}\n");
    } else {
        emit(lam->body());
    }
}

std::string DotEmitter::emit_bb(BB&, const Def* def) {
    if (auto lam = def->isa<Lam>()) return lam->name();

    stream_.fmt("\"{}:{}\" [label=\"", def->node_name(), def->unique_name());
    stream_def_(stream_, def);
    stream_.fmt("\"];\n");

    for (auto op : def->ops()) {
        emit_unsafe(op);
        connections_.fmt("\"{}:{}\"->\"{}:{}\";\n", def->node_name(), def->unique_name(), op->node_name(),
                         op->unique_name());
    }

    return {def->unique_name()};
}

std::string DotEmitter::prepare(const Scope& scope) {
    auto lam = scope.entry()->as_nom<Lam>();

    emit_cluster_start(stream_, lam);

    return lam->name();
}

void DotEmitter::finalize(const Scope&) { stream_ << "}\n"; }

} // namespace thorin::dot
