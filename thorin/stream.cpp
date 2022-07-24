#include "thorin/world.h"

#include "thorin/analyses/deptree.h"
#include "thorin/fe/tok.h"
#include "thorin/util/assert.h"
#include "thorin/util/container.h"

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
        if (var->nom()->num_vars() == 1) return print(os, "{}", var->unique_name());
        return print(os, "@{}", var->nom());
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

//------------------------------------------------------------------------------

class RecStreamer {
public:
    RecStreamer(std::ostream& os, size_t max)
        : os(os)
        , max(max) {}

    void run(const DepNode* node = nullptr);
    void run(const Def*);
    void pattern(const Def*);

    std::ostream& os;
    Tab tab;
    size_t max;
    unique_queue<NomSet> noms;
    DefSet defs;
};

void RecStreamer::run(const Def* def) {
    if (!defs.emplace(def).second) return;

    for (auto op : def->ops()) { // for now, don't include debug info and type
        if (auto nom = op->isa_nom()) {
            if (max != 0) {
                if (noms.push(nom)) --max;
            }
        } else {
            run(op);
        }
    }

    if (auto nom = def->isa_nom()) {
        tab.lnprint(os, "{}", Unwrap(nom));
    } else if (!Unwrap(def)) {
        def->let(os, tab);
    }
}

void RecStreamer::run(const DepNode* node) {
    while (!noms.empty()) {
        auto nom = noms.pop();
        os << std::endl << std::endl;

        auto id = [&](const Def* def) {
            if (def->is_external() || (!def->is_set() && def->isa<Lam>())) return def->name();
            return def->unique_name();
        };

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

        if (nom->is_set()) {
            if (auto lam = nom->isa<Lam>()) {
                tab.print(os, ".lam {} {} ", nom->is_external() ? ".extern " : "", id(nom));
                pattern(lam->var());
                print(os, " -> {} = {{", lam->type()->codom());
                ++tab;
                tab.lnprint(os, "{}", lam->body());
                tab.lnprint(os, "}};");
                --tab;
            } else {
                tab.print(os, "{} {}{}: {}", nom_prefix(nom), nom->is_external() ? ".extern " : "", id(nom),
                          nom->type());
                nom_op0(nom);
                if (nom->var()) {
                    auto e = nom->num_vars();
                    print(os, ", @{}", e == 1 ? "" : "(");
                    range(os, nom->vars(), [&](auto def) {
                        if (def)
                            os << def->unique_name();
                        else
                            os << "<TODO>";
                    });
                    if (e != 1) print(os, ")");
                }
                print(os, " = {{");
                ++tab;

                if (auto lam = nom->isa<Lam>()) {
                    run(lam->filter());
                    tab.lnprint(os, "{},", lam->filter());

                    if (node) {
                        for (auto child : node->children())
                            if (auto nom = child->nom()) {
                                noms.push(nom);
                                run(child);
                            }
                    }
                    run(lam->body());
                    tab.lnprint(os, "{}", lam->body());
                } else {
                    run(nom);
                }

                --tab;
                tab.lnprint(os, "}};");
            }
        } else {
            tab.print(os, "{}: {} = {{ <unset> }};", id(nom), nom->type());
        }
    }
}

void RecStreamer::pattern(const Def* def) {
    if (!def) {
        print(os, "_");
    } else if (def->num_projs() == 1) {
        print(os, "{}: {}", def->unique_name(), def->type());
    } else {
        auto projs = def->projs();
        print(os, "{}::(", def->unique_name());
        range(os, projs, [&](const Def* proj) { pattern(proj); });
        print(os, ")");
    }
}

std::ostream& operator<<(std::ostream& os, const Def* def) {
    if (def == nullptr) return os << "<nullptr>";
    if (Unwrap(def)) return os << Unwrap(def);
    return os << def->unique_name();
}

std::ostream& Def::stream(std::ostream& os, size_t max) const {
    if (max == 0) {
        Tab tab;
        return let(os, tab);
    }

    RecStreamer rec(os, --max);
    if (auto nom = isa_nom()) {
        rec.noms.push(nom);
        rec.run();
    } else {
        rec.run(this);
        if (max != 0) rec.run();
    }

    return os;
}

std::ostream& Def::let(std::ostream& os, Tab& tab) const {
    return tab.lnprint(os, ".let {}: {} = {};", unique_name(), type(), Unwrap(this));
}

//------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, std::pair<const Def*, const Def*> p) {
    return print(os, "({}, {})", p.first, p.second);
}

void Def::dump() const { std::cout << this << std::endl; }
void Def::dump(size_t max) const { stream(std::cout, max) << std::endl; }

// TODO polish this
std::ostream& operator<<(std::ostream& os, const World& world) {
    world.freeze(true);
    auto old_gid = world.curr_gid();
#if 1
    DepTree dep(world);
    RecStreamer rec(os, 0);
    for (auto& import : world.imported()) print(os, ".import {};\n", import);

    for (auto child : dep.root()->children()) world.stream(rec, child) << std::endl;
    assert_unused(old_gid == world.curr_gid());
    world.freeze(false);
    return os;
#else
    RecStreamer rec(s, std::numeric_limits<size_t>::max());
    s << "module '" << name();

    for (const auto& [name, nom] : externals()) {
        rec.noms.push(nom);
        rec.run();
    }

    return s.endl();
#endif
}

std::ostream& World::stream(RecStreamer& rec, const DepNode* n) const {
    if (auto nom = n->nom()) {
        rec.noms.push(nom);
        rec.run(n);
    }

    return rec.os;
}

void World::debug_stream() const {
    if (log.level == Log::Level::Debug) *log.ostream << *this;
}

void World::dump() const { std::cout << *this; }

} // namespace thorin
