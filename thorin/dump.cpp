#include <fstream>

#include "thorin/world.h"

#include "thorin/analyses/deptree.h"
#include "thorin/fe/tok.h"
#include "thorin/util/assert.h"
#include "thorin/util/container.h"

using namespace std::literals;

namespace thorin {

static Def* isa_decl(const Def* def) {
    if (auto nom = def->isa_nom()) {
        if (nom->isa<Lam>() || (!nom->name().empty() && nom->name() != "_"s)) return nom;
    }
    return nullptr;
}

static std::string id(const Def* def) {
    if (def->is_external() || (!def->is_set() && def->isa<Lam>())) return def->name();
    return def->unique_name();
}

static std::string_view external(const Def* def) {
    if (def->is_external()) return ".extern "sv;
    return ""sv;
}

/// This is a wrapper to dump a Def "inline" and print it with all of its operands.
struct Inline {
    Inline(const Def* def, bool dump_gid)
        : def_(def)
        , dump_gid_(dump_gid) {}
    Inline(const Def* def)
        : Inline(def, def->world().flags().dump_gid) {}

    const Def* operator->() const { return def_; };
    const Def* operator*() const { return def_; };
    explicit operator bool() const {
        if (def_->no_dep()) return true;
        if (auto nom = def_->isa_nom()) {
            if (isa_decl(nom)) return false;
            return true;
        }

        if (auto app = def_->isa<App>()) {
            if (app->type()->isa<Pi>()) return true; // curried apps are printed inline
            if (app->type()->isa<Type>()) return true;
            if (app->callee()->isa<Axiom>()) { return app->callee_type()->num_doms() <= 1; }
            return false;
        }

        return true;
    }

    friend std::ostream& operator<<(std::ostream&, Inline);

private:
    const Def* def_;
    const bool dump_gid_;
};

template<bool L>
struct LRPrec {
    LRPrec(const Def* l, const Def* r)
        : l(l)
        , r(r) {}

    friend std::ostream& operator<<(std::ostream& os, const LRPrec& p) {
        if constexpr (L) {
            if (Inline(p.l) && fe::Tok::prec(fe::Tok::prec(p.r))[0] > fe::Tok::prec(p.r)) return print(os, "({})", p.l);
            return print(os, "{}", p.l);
        } else {
            if (Inline(p.r) && fe::Tok::prec(p.l) > fe::Tok::prec(fe::Tok::prec(p.l))[1]) return print(os, "({})", p.r);
            return print(os, "{}", p.r);
        }
    }

private:
    const Def* l;
    const Def* r;
};

using LPrec = LRPrec<true>;
using RPrec = LRPrec<false>;

std::ostream& operator<<(std::ostream& os, Inline u) {
    if (u.dump_gid_) print(os, "/*{}*/", u->gid());

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
        if (auto nom = arr->isa_nom<Arr>(); nom && nom->var())
            return print(os, "«{}: {}; {}»", nom->var(), nom->shape(), nom->body());
        return print(os, "«{}; {}»", arr->shape(), arr->body());
    } else if (auto pack = u->isa<Pack>()) {
        if (auto nom = pack->isa_nom<Pack>(); nom && nom->var())
            return print(os, "‹{}: {}; {}›", nom->var(), nom->shape(), nom->body());
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

/// This will stream @p def as an operand.
/// This is usually Def::unique_name, but in some simple cases it will be displayed Inline%d.
std::ostream& operator<<(std::ostream& os, const Def* def) {
    if (def == nullptr) return os << "<nullptr>";
    if (Inline(def)) return os << Inline(def);
    return os << def->unique_name();
}

std::ostream& let(Tab& tab, std::ostream& os, const Def* def) {
    return tab.print(os, ".let {}: {} = {};\n", def->unique_name(), def->type(), Inline(def, false));
}

//------------------------------------------------------------------------------

class Dumper {
public:
    Dumper(std::ostream& os, size_t max)
        : os(os)
        , max(max) {}

    void recurse(const DepNode*);
    void recurse(const Def*, bool first = false);
    void recurse_(Defs defs) {
        for (auto def : defs) recurse(def);
    }
    void dump_decl(const DepNode*);
    void dump_ptrn(const Def*, const Def*);
    void dump(const DepNode*, Lam*);

    std::ostream& os;
    Tab tab;
    size_t max;
    unique_queue<NomSet> noms;
    DefSet defs;
};

void Dumper::dump_decl(const DepNode* node) {
    auto nom = node->nom();

    if (auto lam = nom->isa<Lam>()) {
        dump(node, lam);
        return;
    }

    auto nom_prefix = [&](const Def* def) {
        if (def->isa<Sigma>()) return ".Sigma";
        if (def->isa<Arr>()) return ".Arr";
        if (def->isa<Pack>()) return ".pack";
        if (def->isa<Pi>()) return ".Pi";
        unreachable();
    };

    auto nom_op0 = [&](const Def* def) -> std::ostream& {
        if (auto sig = def->isa<Sigma>()) return print(os, ", {}", sig->num_ops());
        if (auto arr = def->isa<Arr>()) return print(os, ", {}", arr->shape());
        if (auto pack = def->isa<Pack>()) return print(os, ", {}", pack->shape());
        if (auto pi = def->isa<Pi>()) return print(os, ", {}", pi->dom());
        unreachable();
    };

    if (!nom->is_set()) {
        tab.print(os, "{}: {} = {{ <unset> }};", id(nom), nom->type());
        return;
    }

    tab.print(os, "{} {}{}: {}", nom_prefix(nom), external(nom), id(nom), nom->type());
    nom_op0(nom);
    if (nom->var()) { // TODO rewrite with dedicated methods
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
    tab.println(os, " = {{");
    ++tab;
    recurse(node);
    recurse(nom);
    tab.print(os, "{, }\n", nom->ops());
    --tab;
    tab.print(os, "}};\n");
}

void Dumper::dump(const DepNode* node, Lam* lam) {
    // TODO filter

    auto ptrn = [&](auto&) { dump_ptrn(lam->var(), lam->type()->dom()); };

    if (lam->type()->is_cn()) {
        tab.println(os, ".cn {}{} {} = {{", external(lam), id(lam), ptrn);
    } else {
        tab.println(os, ".lam {}{} {} -> {} = {{", external(lam), id(lam), ptrn, lam->type()->codom());
    }

    ++tab;
    if (lam->is_set()) {
        recurse(node);
        recurse(lam->filter());
        recurse(lam->body(), true);
        tab.print(os, "{}\n", Inline(lam->body()));
    } else {
        tab.print(os, " <unset>\n");
    }
    --tab;
    tab.print(os, "}};\n");
}

void Dumper::recurse(const DepNode* node) {
    if (node) {
        for (auto child : node->children()) {
            if (isa_decl(child->nom())) dump_decl(child);
        }
    }
}

void Dumper::recurse(const Def* def, bool first /*= false*/) {
    if (isa_decl(def)) return;
    if (!defs.emplace(def).second) return;

    for (auto op : def->partial_ops().skip_front()) { // ignore dbg
        if (!op) continue;
        recurse(op);
    }

    if (!first && !Inline(def)) let(tab, os, def);
}

void Dumper::dump_ptrn(const Def* def, const Def* type) {
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

std::ostream& Def::stream(std::ostream& os, size_t /*max*/) const {
#if 0
    World::Freezer freezer(world());
    if (max == 0) {
        Tab tab;
        return let(tab, os, this);
    }

    Dumper dumper(os, --max);
    if (auto nom = isa_nom()) {
        dumper.noms.push(nom);
        dumper.run();
    } else {
        dumper.recurse(this);
        if (max != 0) dumper.run();
    }

#endif
    return os;
}

//------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, std::pair<const Def*, const Def*> p) {
    return print(os, "({}, {})", p.first, p.second);
}

void Def::dump() const { std::cout << this << std::endl; }
void Def::dump(size_t max) const { stream(std::cout, max) << std::endl; }

void World::dump(std::ostream& os) const {
    auto freezer = World::Freezer(*this);
    auto old_gid = curr_gid();
    auto dep     = DepTree(*this);
    auto dumper  = Dumper(os, 0);

    for (const auto& import : imported()) print(os, ".import {};\n", import);
    dumper.recurse(dep.root());

    assertf(old_gid == curr_gid(), "new nodes created during dump. old_gid: {}; curr_gid: {}", old_gid, curr_gid());
}

void World::dump() const { dump(std::cout); }

void World::dump(std::string_view file /*= {}*/) const {
    auto s   = std::string(file.empty() ? file : name());
    auto ofs = std::ofstream(s);
    dump(ofs);
}

void World::debug_dump() const {
    if (log().level == Log::Level::Debug) dump(*log().ostream);
}

} // namespace thorin
