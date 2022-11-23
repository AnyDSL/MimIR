#include "dialects/autodiff/analysis/gradient_analysis.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/analysis/alias_analysis.h"
#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/analysis/cache_analysis.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/utils/builder.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

GradientAnalysis::GradientAnalysis(AnalysisFactory& factory)
    : Analysis(factory)
    , alias_(factory.alias()) {
    run(factory.lam());
}

GradientAnalysis::Lattice& GradientAnalysis::get_lattice(const Def* def) {
    auto it = lattices.find(def);
    if (it == lattices.end()) { it = lattices.emplace(def, std::make_unique<Lattice>(def)).first; }
    return *it->second;
}

DefSet& GradientAnalysis::defs() { return gradient_set; }

bool GradientAnalysis::has_gradient(const Def* def) { return gradient_set.contains(def); }

bool GradientAnalysis::is_const(const Def* def) {
    if (def->isa<Lit>()) {
        return true;
    } else if (auto app = def->isa<App>()) {
        return is_const(app->arg());
    } else if (auto tuple = def->isa<Tuple>()) {
        for (auto op : tuple->ops()) {
            if (!is_const(op)) { return false; }
        }

        return true;
    } else if (auto pack = def->isa<Pack>()) {
        return is_const(pack->body());
    }

    return false;
}

bool GradientAnalysis::meet(const Def* present, const Def* next) {
    auto alias_node = alias_.alias_node(present);
    auto& alias_set = alias_node.alias_set();

    auto& next_lattice = get_lattice(next);
    if (alias_set.empty()) {
        auto lattice = get_lattice(present);

        if (lattice.has_gradient()) { todo_ |= next_lattice.set_gradient(); }
    } else {
        for (auto node : alias_set) {
            auto lattice = get_lattice(node->def());

            if (lattice.has_gradient()) {
                todo_ |= next_lattice.set_gradient();
                break;
            }
        }
    }
}

bool GradientAnalysis::meet_projs(const Def* present, const Def* next) {
    auto alias_node = alias_.alias_node(present);
    auto& alias_set = alias_node.alias_set();

    auto& next_lattice = get_lattice(next);
    if (alias_set.empty()) {
        auto lattice = get_lattice(present);

        if (lattice.has_gradient()) {
            for (auto proj : next->projs()) { todo_ |= next_lattice.set_gradient(); }
        }
    } else {
        for (auto node : alias_set) {
            auto lattice = get_lattice(node->def());

            if (lattice.has_gradient()) {
                for (auto proj : next->projs()) { todo_ |= next_lattice.set_gradient(); }
                break;
            }
        }
    }
}

bool GradientAnalysis::visit(const Def* def) {
    if (auto tuple = def->isa<Tuple>()) {
        return meet_projs(tuple, tuple);
    } else if (auto pack = def->isa<Pack>()) {
        return meet(pack, pack->body());
    } else if (auto app = def->isa<App>()) {
        auto arg = app->arg();

        if (match<math::arith>(app) || match<math::extrema>(app) || match<math::exp>(app) || match<math::gamma>(app) ||
            match<math::tri>(app) || match<math::rt>(app)) {
            return meet(app, arg);
        } else if (auto lea = match<mem::lea>(app)) {
            auto arr = arg->proj(0);
            auto idx = arg->proj(1);
            return meet(lea, arr) || meet(arr, lea);
        } else if (auto store = match<mem::store>(app)) {
            auto ptr = arg->proj(1);
            auto val = arg->proj(2);

            return meet(ptr, val);
        } else if (auto load = match<mem::load>(app)) {
            auto val = load->proj(1);
            auto ptr = arg->proj(1);
            return meet(val, ptr);
        } else if (auto bitcast = match<core::bitcast>(app)) {
            return meet(bitcast, bitcast->arg());
        }
    }

    return true;
}

void GradientAnalysis::run(Lam* diffee) {
    for (auto var : diffee->vars()) {
        auto& lattice = get_lattice(var);
        lattice.set_gradient();
    }

    auto& utils = factory().utils();

    todo_ = true;
    while (todo_) {
        todo_ = false;
        for (auto lam : utils.lams()) {
            for (auto def : utils.scope(lam).bound()) { visit(def); }
        }
    }

    for (auto& [def, lattice] : lattices) {
        if (lattice->has_gradient()) { gradient_set.insert(def); }
    }
}

} // namespace thorin::autodiff
