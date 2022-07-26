#include "thorin/world.h"

#include "thorin/analyses/deptree.h"
#include "thorin/fe/tok.h"
#include "thorin/util/assert.h"
#include "thorin/util/container.h"

using namespace std::literals;

namespace thorin {

struct Unwrap {
    Unwrap(const Def* def)
        : def(def) {}

    const Def* operator->() const { return def; };
    const Def* operator*() const { return def; };
    operator bool() const {
        if (def->isa_nom()) return false;
        if (def->isa<Global>()) return false;
        // if (def->no_dep()) return true;
        if (auto app = def->isa<App>()) {
            if (app->type()->isa<Pi>()) return true; // curried apps are printed inline
            if (app->type()->isa<Type>()) return true;
            if (app->callee()->isa<Axiom>()) { return app->callee_type()->num_doms() <= 1; }
            return false;
        }
        return true;
    }

    const Def* def;
};

// TODO can we get rid off this?
static fe::Tok::Prec prec(const Def* def) {
    if (def->isa<Pi>()) return fe::Tok::Prec::Arrow;
    if (def->isa<App>()) return fe::Tok::Prec::App;
    if (def->isa<Extract>()) return fe::Tok::Prec::Extract;
    if (def->isa<Lit>()) return fe::Tok::Prec::Lit;
    return fe::Tok::Prec::Bot;
}

static fe::Tok::Prec prec_l(const Def* def) {
    assert(!def->isa<Lit>());
    if (def->isa<Pi>()) return fe::Tok::Prec::App;
    if (def->isa<App>()) return fe::Tok::Prec::App;
    if (def->isa<Extract>()) return fe::Tok::Prec::Extract;
    return fe::Tok::Prec::Bot;
}

static fe::Tok::Prec prec_r(const Def* def) {
    if (def->isa<Pi>()) return fe::Tok::Prec::Arrow;
    if (def->isa<App>()) return fe::Tok::Prec::Extract;
    if (def->isa<Extract>()) return fe::Tok::Prec::Lit;
    return fe::Tok::Prec::Bot;
}

template<bool L>
struct LRPrec {
    LRPrec(const Def* l, const Def* r)
        : l_(l)
        , r_(r) {}

    friend std::ostream& operator<<(std::ostream& os, const LRPrec& p) {
        if constexpr (L) {
            if (Unwrap(p.l_) && prec_l(p.r_) > prec(p.r_)) return print(os, "({})", p.l_);
            return print(os, "{}", p.l_);
        } else {
            if (Unwrap(p.r_) && prec(p.l_) > prec_r(p.l_)) return print(os, "({})", p.r_);
            return print(os, "{}", p.r_);
        }
    }

private:
    const Def* l_;
    const Def* r_;
};

using LPrec = LRPrec<true>;
using RPrec = LRPrec<false>;

std::ostream& operator<<(std::ostream& os, Unwrap u) {
    if (auto type = u->isa<Type>()) {
        auto level = as_lit(type->level()); // TODO other levels
        return print(os, level == 0 ? "★" : "□");
    } else if (u->isa<Nat>()) {
        return print(os, ".Nat");
    } else if (auto bot = u->isa<Bot>()) {
        return print(os, "⊥:{}", bot->type());
    } else if (auto top = u->isa<Top>()) {
        return print(os, "⊤:{}", top->type());
    } else if (auto axiom = u->isa<Axiom>()) {
        const auto& name = axiom->debug().name;
        return print(os, "{}{}", name[0] == '%' ? "" : "%", name);
    } else if (auto lit = u->isa<Lit>()) {
        if (auto real = thorin::isa<Tag::Real>(lit->type())) {
            switch (as_lit(real->arg())) {
                case 16: return print(os, "{}:(%Real 16)", lit->get<r16>());
                case 32: return print(os, "{}:(%Real 32)", lit->get<r32>());
                case 64: return print(os, "{}:(%Real 64)", lit->get<r64>());
                default: unreachable();
            }
        }
        return print(os, "{}:{}", lit->get(), lit->type());
    } else if (auto ex = u->isa<Extract>()) {
        if (ex->tuple()->isa<Var>() && ex->index()->isa<Lit>()) return print(os, "{}", ex->unique_name());
        return print(os, "{}#{}", ex->tuple(), ex->index());
    } else if (auto var = u->isa<Var>()) {
        return print(os, "{}", var->unique_name());
    } else if (auto pi = u->isa<Pi>()) {
        if (pi->is_cn()) {
            return print(os, ".Cn {}", pi->dom());
        } else {
            return print(os, "{} -> {}", pi->dom(), pi->codom());
        }
    } else if (auto lam = u->isa<Lam>()) {
        return print(os, "{}, {}", lam->filter(), lam->body());
    } else if (auto app = u->isa<App>()) {
        if (auto size = isa_lit(isa_sized_type(app))) {
            if (auto real = thorin::isa<Tag::Real>(app)) return print(os, "(%Real {})", *size);
            if (auto _int = thorin::isa<Tag::Int>(app)) return print(os, "(%Int {})", *size);
            unreachable();
        }
        return print(os, "{} {}", LPrec(app->callee(), app), RPrec(app, app->arg()));
    } else if (auto sigma = u->isa<Sigma>()) {
        return print(os, "[{, }]", sigma->ops());
    } else if (auto tuple = u->isa<Tuple>()) {
        print(os, "({, })", tuple->ops());
        return tuple->type()->isa_nom() ? print(os, ":{}", tuple->type()) : os;
    } else if (auto arr = u->isa<Arr>()) {
        return print(os, "«{}; {}»", arr->shape(), arr->body());
    } else if (auto pack = u->isa<Pack>()) {
        return print(os, "‹{}; {}›", pack->shape(), pack->body());
    } else if (auto proxy = u->isa<Proxy>()) {
        return print(os, ".proxy#{}#{} {, }", proxy->pass(), proxy->tag(), proxy->ops());
    } else if (auto bound = isa_bound(*u)) {
        auto op = bound->isa<Join>() ? "∪" : "∩";
        if (auto nom = u->isa_nom()) print(os, "{}{}: {}", op, nom->unique_name(), nom->type());
        return print(os, "{}({, })", op, bound->ops());
    }

    // other
    if (u->flags() == 0) return print(os, ".{} ({, })", u->node_name(), u->ops());
    return print(os, ".{}#{} ({, })", u->node_name(), u->flags(), u->ops());
}

std::ostream& let(Tab& tab, std::ostream& os, const Def* def) {
    return tab.lnprint(os, ".let {}: {} = {};", def->unique_name(), def->type(), Unwrap(def));
}

//------------------------------------------------------------------------------

class RecDumper {
public:
    RecDumper(std::ostream& os, size_t max)
        : os(os)
        , max(max) {}

    void run(const DepNode* node = nullptr);
    void rec(const Def*);
    void rec(Defs defs) {
        for (auto def : defs) rec(def);
    }
    void dump(const DepNode* n);
    void dump_ptrn(const Def*, const Def*);
    void dump(const DepNode*, Lam*);

    static std::string id(const Def* def) {
        if (def->is_external() || (!def->is_set() && def->isa<Lam>())) return def->name();
        return def->unique_name();
    }

    static std::string_view external(const Def* def) {
        if (def->is_external()) return ".extern "sv;
        return ""sv;
    }

    std::ostream& os;
    Tab tab;
    size_t max;
    unique_queue<NomSet> noms;
    DefSet defs;
};

void RecDumper::run(const DepNode* node) {
    while (!noms.empty()) {
        auto nom = noms.pop();
        os << std::endl << std::endl;

        if (auto lam = nom->isa<Lam>()) {
            dump(node, lam);
            continue;
        }

        auto nom_prefix = [&](const Def* def) {
            if (def->isa<Lam>()) return ".lam";
            if (def->isa<Sigma>()) return ".Sigma";
            if (def->isa<Arr>()) return ".Arr";
            if (def->isa<Pack>()) return ".pack";
            if (def->isa<Pi>()) return ".Pi";
            unreachable();
        };

        auto nom_op0 = [&](const Def* def) -> std::ostream& {
            if (def->isa<Lam>()) return os;
            if (auto sig = def->isa<Sigma>()) return print(os, ", {}", sig->num_ops());
            if (auto arr = def->isa<Arr>()) return print(os, ", {}", arr->shape());
            if (auto pack = def->isa<Pack>()) return print(os, ", {}", pack->shape());
            if (auto pi = def->isa<Pi>()) return print(os, ", {}", pi->dom());
            unreachable();
        };

        if (!nom->is_set()) {
            tab.print(os, "{}: {} = {{ <unset> }};", id(nom), nom->type());
            continue;
        }

        tab.print(os, "{} {}{}: {}", nom_prefix(nom), external(nom), id(nom), nom->type());
        nom_op0(nom);
        if (nom->var()) {
            if (auto e = nom->num_vars(); e != 1) {
                print(os, "{, }", Elem(nom->vars(), [&](auto def) {
                          if (def)
                              os << def->unique_name();
                          else
                              os << "<TODO>";
                      }));
            } else {
                print(os, ", @{}", nom->var()->unique_name());
            }
        }
        print(os, " = {{");
        ++tab;

        if (auto lam = nom->isa<Lam>()) {
            rec(lam->filter());
            tab.lnprint(os, "{},", lam->filter());

            if (node) {
                for (auto child : node->children())
                    if (auto nom = child->nom()) {
                        noms.push(nom);
                        run(child);
                    }
            }
            rec(lam->body());
            tab.lnprint(os, "{}", lam->body());
        } else {
            rec(nom);
        }

        --tab;
        tab.lnprint(os, "}};");
    }
}

void RecDumper::dump(const DepNode* n) {
    if (auto nom = n->nom()) {
        noms.push(nom);
        run(n);
    }
}

void RecDumper::rec(const Def* def) {
    if (!defs.emplace(def).second) return;

    for (auto op : def->ops()) { // for now, don't include debug info and type
        if (auto nom = op->isa_nom()) {
            if (max != 0) {
                if (noms.push(nom)) --max;
            }
        } else {
            rec(op);
        }
    }

    if (auto nom = def->isa_nom()) {
        tab.lnprint(os, "{}", Unwrap(nom));
    } else if (!Unwrap(def)) {
        let(tab, os, def);
    }
}

void RecDumper::dump_ptrn(const Def* def, const Def* type) {
    if (!def) {
        os << type;
    } else {
        auto projs = def->projs();
        if (projs.size() == 1 || std::ranges::all_of(projs, [](auto def) { return !def; })) {
            print(os, "{}: {}", def->unique_name(), type);
        } else {
            size_t i = 0;
            print(os, "{}::[{, }]", def->unique_name(),
                  Elem(projs, [&](auto proj) { dump_ptrn(proj, type->proj(i++)); }));
        }
    }
}

void RecDumper::dump(const DepNode* node, Lam* lam) {
    // TODO filter
    auto ptrn = [&]() { dump_ptrn(lam->var(), lam->type()->dom()); };
    if (lam->type()->is_cn()) {
        tab.print(os, ".cn {}{} {} = {{", external(lam), id(lam), ptrn);
    } else {
        tab.print(os, ".lam {}{} {} -> {} = {{", external(lam), id(lam), ptrn, lam->type()->codom());
    }

    ++tab;
    if (node) {
        for (auto child : node->children())
            if (auto nom = child->nom()) {
                noms.push(nom);
                run(child);
            }
    }
    if (lam->is_set()) {
        rec(lam->ops());
        tab.lnprint(os, "{}", lam->body());
    } else {
        tab.lnprint(os, " <unset> ");
    }
    --tab;
    tab.lnprint(os, "}};");
}

std::ostream& operator<<(std::ostream& os, const Def* def) {
    if (def == nullptr) return os << "<nullptr>";
    if (Unwrap(def)) return os << Unwrap(def);
    return os << def->unique_name();
}

std::ostream& Def::stream(std::ostream& os, size_t max) const {
    if (max == 0) {
        Tab tab;
        return let(tab, os, this);
    }

    RecDumper rec(os, --max);
    if (auto nom = isa_nom()) {
        rec.noms.push(nom);
        rec.run();
    } else {
        rec.rec(this);
        if (max != 0) rec.run();
    }

    return os;
}

//------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, std::pair<const Def*, const Def*> p) {
    return print(os, "({}, {})", p.first, p.second);
}

void Def::dump() const { std::cout << this << std::endl; }
void Def::dump(size_t max) const { stream(std::cout, max) << std::endl; }

void World::dump(std::ostream& os) const {
    freeze(true);
    auto old_gid = curr_gid();

    DepTree dep(*this);
    RecDumper rec(os, 0);
    for (const auto& import : imported()) print(os, ".import {};\n", import);

    for (auto child : dep.root()->children()) {
        rec.dump(child);
        os << std::endl;
    }

    assert_unused(old_gid == curr_gid());
    freeze(false);
}

void World::dump() const { dump(std::cout); }

void World::debug_dump() const {
    if (log().level == Log::Level::Debug) dump(*log().ostream);
}

} // namespace thorin
