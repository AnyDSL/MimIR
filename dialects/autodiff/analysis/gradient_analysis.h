#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class AliasAnalysis;

struct GradientLattice {
    enum Type { Bot = 0, Required = 1, Has = 2, Top = 3 };

    GradientLattice(const Def* def)
        : type_(Type::Bot)
        , def_(def) {}

    GradientLattice(GradientLattice& other) = delete;

    Type type() { return type_; }

    bool set(Type type);

private:
    Type type_;
    const Def* def_;
};

inline GradientLattice::Type operator|(GradientLattice::Type a, GradientLattice::Type b) {
    return static_cast<GradientLattice::Type>(static_cast<int>(a) | static_cast<int>(b));
}
inline GradientLattice::Type operator&(GradientLattice::Type a, GradientLattice::Type b) {
    return static_cast<GradientLattice::Type>(static_cast<int>(a) & static_cast<int>(b));
}

class AffineCFNode;
class GradientAnalysis : public Analysis {
public:
private:
    bool todo_;
    DefSet gradient_set;
    DefMap<std::unique_ptr<GradientLattice>> lattices;

public:
    GradientAnalysis(AnalysisFactory& factory);
    GradientAnalysis(GradientAnalysis& other) = delete;

    GradientLattice& get_lattice(const Def* def);

    DefSet& defs();

    bool has_gradient(const Def* def);

    bool is_const(const Def* def);

    void meet_app(const Def* arg, AffineCFNode* node);
    void meet(GradientLattice& present, GradientLattice& next);
    void meet(const Def* present, const Def* next);
    void meet_projs(const Def* present, const Def* next);

    void visit(const Def* def);

    void run(Lam* diffee);
};

} // namespace thorin::autodiff
