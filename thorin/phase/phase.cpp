#include "thorin/phase/phase.h"

namespace thorin {

void Phase::run() {
    world().ILOG("=== {}: start ===", name());
    start();
    world().ILOG("=== {}: done ===", name());
}

void RewritePhase::start() {
    for (const auto& [_, ax] : world().axioms()) rewrite(ax);
    for (const auto& [_, nom] : world().externals()) rewrite(nom)->as_nom()->make_external();

    swap(world_, new_);
}

const Def* RewritePhase::rewrite(const Def* old_def) {
    if (auto i = old2new_.find(old_def); i != old2new_.end()) return i->second;

    if (auto [pre, recurse] = pre_rewrite(old_def); pre) {
        auto new_def = recurse ? rewrite(pre) : pre;
        return old2new_[pre] = new_def;
    }

    auto new_type = old_def->type() ? rewrite(old_def->type()) : nullptr;
    auto new_dbg  = old_def->dbg() ? rewrite(old_def->dbg()) : nullptr;

    if (auto infer = old_def->isa_nom<Infer>()) {
        if (auto op = infer->op()) return op;
    }

    if (auto old_nom = old_def->isa_nom()) {
        auto new_nom     = old_nom->stub(new_, new_type, new_dbg);
        old2new_[old_nom] = new_nom;

        for (size_t i = 0, e = old_nom->num_ops(); i != e; ++i) {
            if (auto old_op = old_nom->op(i)) new_nom->set(i, rewrite(old_op));
        }

        if (auto new_def = new_nom->restructure()) return old2new_[old_nom] = new_def;

        return new_nom;
    }

    DefArray new_ops(old_def->num_ops(), [&](auto i) { return rewrite(old_def->op(i)); });
    auto new_def = old_def->rebuild(new_, new_type, new_ops, new_dbg);

    if (auto [post, recurse] = pre_rewrite(old_def); post) {
        auto new_def = recurse ? rewrite(post) : post;
        return old2new_[post] = new_def;
    }

    return old2new_[old_def] = new_def;
}

}
