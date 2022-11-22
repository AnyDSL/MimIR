#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class AliasAnalysis;
class FlowAnalysis : public Analysis {
    struct Lattice {
        enum Type { Bot, Flow };

        Lattice(const Def* def)
            : type_(Type::Bot)
            , def_(def) {}

        Type type_;
        const Def* def_;

        Type type() { return type_; }

        bool set_flow() {
            if (type_ == Bot) {
                type_ = Flow;
                return true;
            }

            return false;
        }

        bool is_flow() { return type_ = Flow; }
    };

    bool todo_;
    DefSet flow_set;
    DefMap<std::unique_ptr<Lattice>> lattices;
    AliasAnalysis& alias_;

public:
    FlowAnalysis(AnalysisFactory& factory);
    FlowAnalysis(FlowAnalysis& other) = delete;

    Lattice& get_lattice(const Def* def);

    DefSet& flow_defs();

    bool isa_flow_def(const Def* def);

    bool is_const(const Def* def);

    bool meet(const Def* present, const Def* next);

    bool meet_projs(const Def* present, const Def* next);

    bool visit(const Def* def);

    void run(Lam* diffee);
};

} // namespace thorin::autodiff
