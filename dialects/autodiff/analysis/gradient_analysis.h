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
class GradientAnalysis : public Analysis {
    struct Lattice {
        enum Type { Bot, Has_Gradient };

        Lattice(const Def* def)
            : type_(Type::Bot)
            , def_(def) {}

        Type type_;
        const Def* def_;

        bool set_gradient() {
            if (type_ == Bot) {
                type_ = Has_Gradient;
                return true;
            }

            return false;
        }

        bool has_gradient() { return type_ = Has_Gradient; }
    };

    bool todo_;
    DefSet gradient_set;
    DefMap<std::unique_ptr<Lattice>> lattices;
    AliasAnalysis& alias_;

public:
    GradientAnalysis(AnalysisFactory& factory);
    GradientAnalysis(GradientAnalysis& other) = delete;

    Lattice& get_lattice(const Def* def);

    DefSet& defs();

    bool has_gradient(const Def* def);

    bool is_const(const Def* def);

    bool meet(const Def* present, const Def* next);

    bool meet_projs(const Def* present, const Def* next);

    bool visit(const Def* def);

    void run(Lam* diffee);
};

} // namespace thorin::autodiff
