#include <fstream>

#include <fe/assert.h>

#include "mim/driver.h"
#include "mim/nest.h"

#include "mim/ast/tok.h"
#include "mim/util/util.h"

using namespace std::literals;

// During dumping, we classify Defs according to the following logic:
// * Inline: These Defs are *always* displayed with all of its operands "inline".
//   E.g.: (1, 2, 3).
// * All other Defs are referenced by its name/unique_name (see id) when they appear as an operand.
// * Mutables are either classifed as "decl" (see isa_decl).
//   In this case, recursing through the Defs' operands stops and this particular Decl is dumped as its own thing.
// * Or - if they are not a "decl" - they are basicallally handled like immutables.

namespace mim {

namespace {

Def* isa_decl(const Def* def) {
    if (auto mut = def->isa_mut()) {
        if (mut->is_external() || mut->isa<Lam>() || (mut->sym() && mut->sym() != '_')) return mut;
    }
    return nullptr;
}

std::string id(const Def* def) {
    if (def->is_external() || (!def->is_set() && def->isa<Lam>())) return def->sym().str();
    return def->unique_name();
}

std::string_view external(const Def* def) {
    if (def->is_external()) return "extern "sv;
    return ""sv;
}

/*
 * Inline + LRPrec
 */

/// This is a wrapper to dump a Def "inline" and print it with all of its operands.
struct Inline {
    Inline(const Def* def, int dump_gid)
        : def_(def)
        , dump_gid_(dump_gid) {}
    Inline(const Def* def)
        : Inline(def, def->world().flags().dump_gid) {}

    const Def* operator->() const { return def_; }
    const Def* operator*() const { return def_; }
    explicit operator bool() const {
        if (def_->has_const_dep()) return true;

        if (auto mut = def_->isa_mut()) {
            if (isa_decl(mut)) return false;
            return true;
        }

        if (auto app = def_->isa<App>()) {
            if (app->type()->isa<Pi>()) return true; // curried apps are printed inline
            if (app->type()->isa<Type>()) return true;
            if (app->callee()->isa<Axiom>()) return app->callee_type()->num_doms() <= 1;
            return false;
        }

        return true;
    }

private:
    const Def* def_;
    const int dump_gid_;

    friend std::ostream& operator<<(std::ostream&, Inline);
};

#define MY_PREC(m)                                                                                              \
    /* left     prec,    right  */                                                                              \
    m(Nil, Bot, Nil) m(Nil, Nil, Nil) m(Lam, Arrow, Arrow) m(Nil, Lam, Pi) m(Nil, Pi, App) m(App, App, Extract) \
        m(Extract, Extract, Lit) m(Nil, Lit, Lit)

enum class MyPrec {
#define CODE(l, p, r) p,
    MY_PREC(CODE)
#undef CODE
};

static constexpr std::array<MyPrec, 2> my_prec(MyPrec p) {
    switch (p) {
#define CODE(l, p, r) \
    case MyPrec::p: return {MyPrec::l, MyPrec::r};
        default: fe::unreachable(); MY_PREC(CODE)
#undef CODE
    }
}

// clang-format off
MyPrec my_prec(const Def* def) {
    if (def->isa<Pi     >()) return MyPrec::Arrow;
    if (def->isa<App    >()) return MyPrec::App;
    if (def->isa<Extract>()) return MyPrec::Extract;
    if (def->isa<Lit    >()) return MyPrec::Lit;
    return MyPrec::Bot;
}
// clang-format on

// TODO prec is currently broken
template<bool L> struct LRPrec {
    LRPrec(const Def* l, const Def* r)
        : l(l)
        , r(r) {}

private:
    const Def* l;
    const Def* r;

    friend std::ostream& operator<<(std::ostream& os, const LRPrec& p) {
        if constexpr (L) {
            if (Inline(p.l) && my_prec(my_prec(p.r))[0] > my_prec(p.r)) return print(os, "({})", p.l);
            return print(os, "{}", p.l);
        } else {
            if (Inline(p.r) && my_prec(p.l) > my_prec(my_prec(p.l))[1]) return print(os, "({})", p.r);
            return print(os, "{}", p.r);
        }
    }
};

using LPrec = LRPrec<true>;
using RPrec = LRPrec<false>;

std::ostream& operator<<(std::ostream& os, Inline u) {
    if (u.dump_gid_ == 2 || (u.dump_gid_ == 1 && !u->isa<Var>() && u->num_ops() != 0)) print(os, "/*{}*/", u->gid());
    if (auto mut = u->isa_mut(); mut && !mut->is_set()) return os << "unset";

    bool ascii = u->world().flags().ascii;
    auto arw   = ascii ? "->" : "→";
    auto al    = ascii ? "<<" : "«";
    auto ar    = ascii ? ">>" : "»";
    auto pl    = ascii ? "(<" : "‹";
    auto pr    = ascii ? ">)" : "›";

    if (auto type = u->isa<Type>()) {
        if (auto level = Lit::isa(type->level()); level && !ascii) {
            if (level == 0) return print(os, "★");
            if (level == 1) return print(os, "□");
        }
        return print(os, "(Type {})", type->level());
    } else if (u->isa<Nat>()) {
        return print(os, "Nat");
    } else if (u->isa<Idx>()) {
        return print(os, "Idx");
    } else if (auto ext = u->isa<Ext>()) {
        auto x = ext->isa<Bot>() ? (ascii ? ".bot" : "⊥") : (ascii ? ".top" : "⊤");
        if (ext->type()->isa<Nat>()) return print(os, "{}:{}", x, ext->type());
        return print(os, "{}:({})", x, ext->type());
    } else if (auto top = u->isa<Top>()) {
        return print(os, "{}:({})", ascii ? ".top" : "⊤", top->type());
    } else if (auto axiom = u->isa<Axiom>()) {
        const auto name = axiom->sym();
        return print(os, "{}{}", name[0] == '%' ? "" : "%", name);
    } else if (auto lit = u->isa<Lit>()) {
        if (lit->type()->isa<Nat>()) {
            switch (lit->get()) {
                    // clang-format off
                case 0x0'0000'0100_n: return os << "i8";
                case 0x0'0001'0000_n: return os << "i16";
                case 0x1'0000'0000_n: return os << "i32";
                // clang-format on
                default: return print(os, "{}", lit->get());
            }
        } else if (auto size = Idx::isa(lit->type())) {
            if (auto s = Lit::isa(size)) {
                switch (*s) {
                        // clang-format off
                    case 0x0'0000'0002_n: return os << (lit->get<bool>() ? "tt" : "ff");
                    case 0x0'0000'0100_n: return os << lit->get() << "I8";
                    case 0x0'0001'0000_n: return os << lit->get() << "I16";
                    case 0x1'0000'0000_n: return os << lit->get() << "I32";
                    case             0_n: return os << lit->get() << "I64";
                    default: {
                        os << lit->get();
                        std::vector<uint8_t> digits;
                        for (auto z = *s; z; z /= 10) digits.emplace_back(z % 10);

                        if (ascii) {
                            os << '_';
                            for (auto d : digits | std::ranges::views::reverse)
                                os << char('0' + d);
                        } else {
                            for (auto d : digits | std::ranges::views::reverse)
                                os << uint8_t(0xE2) << uint8_t(0x82) << (uint8_t(0x80 + d));
                        }
                        return os;
                    }
                        // clang-format on
                }
            }
        }
        if (lit->type()->isa<App>()) return print(os, "{}:({})", lit->get(), lit->type()); // HACK prec magic is broken
        return print(os, "{}:{}", lit->get(), lit->type());
    } else if (auto ex = u->isa<Extract>()) {
        if (ex->tuple()->isa<Var>() && ex->index()->isa<Lit>()) return print(os, "{}", ex->unique_name());
        return print(os, "{}#{}", ex->tuple(), ex->index());
    } else if (auto var = u->isa<Var>()) {
        return print(os, "{}", var->unique_name());
    } else if (auto pi = u->isa<Pi>()) {
        /*return os << "TODO";*/
        if (Pi::isa_cn(pi)) return print(os, "Cn {}", pi->dom());
        if (auto mut = pi->isa_mut<Pi>(); mut && mut->var())
            return print(os, "Π {}: {} {} {}", mut->var(), pi->dom(), arw, pi->codom());
        return print(os, "Π {} {} {}", pi->dom(), arw, pi->codom());
    } else if (auto lam = u->isa<Lam>()) {
        return print(os, "{}, {}", lam->filter(), lam->body());
    } else if (auto app = u->isa<App>()) {
        if (auto size = Idx::isa(app)) {
            if (auto l = Lit::isa(size)) {
                // clang-format off
                switch (*l) {
                    case 0x0'0000'0002_n: return os << "Bool";
                    case 0x0'0000'0100_n: return os << "I8";
                    case 0x0'0001'0000_n: return os << "I16";
                    case 0x1'0000'0000_n: return os << "I32";
                    case             0_n: return os << "I64";
                    default: break;
                }
                // clang-format on
            }
        }
        return print(os, "{} {}", LPrec(app->callee(), app), RPrec(app, app->arg()));
    } else if (auto sigma = u->isa<Sigma>()) {
        if (auto mut = sigma->isa_mut<Sigma>(); mut && mut->var()) {
            size_t i = 0;
            return print(os, "[{, }]", Elem(sigma->ops(), [&](auto op) {
                             if (auto v = mut->var(i++))
                                 print(os, "{}: {}", v, op);
                             else
                                 os << op;
                         }));
        }
        return print(os, "[{, }]", sigma->ops());
    } else if (auto tuple = u->isa<Tuple>()) {
        print(os, "({, })", tuple->ops());
        return tuple->type()->isa_mut() ? print(os, ":{}", tuple->type()) : os;
    } else if (auto arr = u->isa<Arr>()) {
        if (auto mut = arr->isa_mut<Arr>(); mut && mut->var())
            return print(os, "{}{}: {}; {}{}", al, mut->var(), mut->shape(), mut->body(), ar);
        return print(os, "{}{}; {}{}", al, arr->shape(), arr->body(), ar);
    } else if (auto pack = u->isa<Pack>()) {
        if (auto mut = pack->isa_mut<Pack>(); mut && mut->var())
            return print(os, "{}{}: {}; {}{}", pl, mut->var(), mut->shape(), mut->body(), pr);
        return print(os, "{}{}; {}{}", pl, pack->shape(), pack->body(), pr);
    } else if (auto proxy = u->isa<Proxy>()) {
        return print(os, ".proxy#{}#{} {, }", proxy->pass(), proxy->tag(), proxy->ops());
    } else if (auto bound = u->isa<Bound>()) {
        auto op = bound->isa<Join>() ? "∪" : "∩"; // TODO ascii
        if (auto mut = u->isa_mut()) print(os, "{}{}: {}", op, mut->unique_name(), mut->type());
        return print(os, "{}({, })", op, bound->ops());
    }

    // other
    if (u->flags() == 0) return print(os, "(.{} {, })", u->node_name(), u->ops());
    return print(os, "(.{}#{} {, })", u->node_name(), u->flags(), u->ops());
}

/*
 * Dumper
 */

/// This thing operates in two modes:
/// 1. The output of decls is driven by the Nest.
/// 2. Alternatively, decls are output as soon as they appear somewhere during recurse%ing.
///     Then, they are pushed to Dumper::muts.
class Dumper {
public:
    Dumper(std::ostream& os, const Nest* nest = nullptr)
        : os(os)
        , nest(nest) {}

    void dump(Def*);
    void dump(Lam*);
    void dump_let(const Def*);
    void dump_ptrn(const Def*, const Def*);
    void recurse(const Nest::Node*);
    void recurse(const Def*, bool first = false);

    std::ostream& os;
    const Nest* nest;
    Tab tab;
    unique_queue<MutSet> muts;
    DefSet defs;
};

void Dumper::dump(Def* mut) {
    if (auto lam = mut->isa<Lam>()) {
        dump(lam);
        return;
    }

    auto mut_prefix = [&](const Def* def) {
        if (def->isa<Sigma>()) return "Sigma";
        if (def->isa<Arr>()) return ".Arr";
        if (def->isa<Pack>()) return ".pack";
        if (def->isa<Pi>()) return ".Pi";
        if (def->isa<Infer>()) return ".Infer";
        fe::unreachable();
    };

    auto mut_op0 = [&](const Def* def) -> std::ostream& {
        if (auto sig = def->isa<Sigma>()) return print(os, ", {}", sig->num_ops());
        if (auto arr = def->isa<Arr>()) return print(os, ", {}", arr->shape());
        if (auto pack = def->isa<Pack>()) return print(os, ", {}", pack->shape());
        if (auto pi = def->isa<Pi>()) return print(os, ", {}", pi->dom());
        if (auto infer = def->isa<Infer>()) return infer->is_set() ? print(os, ", {}", infer->op()) : print(os, ", ??");
        fe::unreachable();
    };

    if (!mut->is_set()) {
        tab.print(os, "{}: {} = {{ <unset> }};", id(mut), mut->type());
        return;
    }

    tab.print(os, "{} {}{}: {}", mut_prefix(mut), external(mut), id(mut), mut->type());
    mut_op0(mut);
    if (mut->var()) { // TODO rewrite with dedicated methods
        if (auto e = mut->num_vars(); e != 1) {
            print(os, "{, }", Elem(mut->vars(), [&](auto def) {
                      if (def)
                          os << def->unique_name();
                      else
                          os << "<TODO>";
                  }));
        } else {
            print(os, ", @{}", mut->var()->unique_name());
        }
    }
    tab.println(os, " = {{");
    ++tab;
    if (nest) recurse(nest->mut2node(mut));
    recurse(mut);
    tab.print(os, "{, }\n", mut->ops());
    --tab;
    tab.print(os, "}};\n");
}

void Dumper::dump(Lam* lam) {
    // TODO filter
    auto ptrn = [&](auto&) { dump_ptrn(lam->var(), lam->type()->dom()); };

    if (Lam::isa_cn(lam))
        tab.println(os, "con {}{} {}@({}) = {{", external(lam), id(lam), ptrn, lam->filter());
    else
        tab.println(os, "lam {}{} {}: {} = {{", external(lam), id(lam), ptrn, lam->type()->codom());

    ++tab;
    if (lam->is_set()) {
        if (nest) recurse(nest->mut2node(lam));
        recurse(lam->filter());
        recurse(lam->body(), true);
        if (lam->body()->isa_mut())
            tab.print(os, "{}\n", lam->body());
        else
            tab.print(os, "{}\n", Inline(lam->body()));
    } else {
        tab.print(os, " <unset>\n");
    }
    --tab;
    tab.print(os, "}};\n");
}

void Dumper::dump_let(const Def* def) {
    tab.print(os, "let {}: {} = {};\n", def->unique_name(), def->type(), Inline(def, 0));
}

void Dumper::dump_ptrn(const Def* def, const Def* type) {
    if (!def) {
        os << type;
    } else {
        auto projs = def->tprojs();
        if (projs.size() == 1 || std::ranges::all_of(projs, [](auto def) { return !def; })) {
            print(os, "{}: {}", def->unique_name(), type);
        } else {
            size_t i = 0;
            print(os, "{}::[{, }]", def->unique_name(),
                  Elem(projs, [&](auto proj) { dump_ptrn(proj, type->proj(i++)); }));
        }
    }
}

void Dumper::recurse(const Nest::Node* node) {
    for (auto child : node->child_muts())
        if (auto mut = isa_decl(child)) dump(mut);
}

void Dumper::recurse(const Def* def, bool first /*= false*/) {
    if (auto mut = isa_decl(def)) {
        if (!nest) muts.push(mut);
        return;
    }

    if (!defs.emplace(def).second) return;

    for (auto op : def->deps()) recurse(op);

    if (!first && !Inline(def)) dump_let(def);
}

} // namespace

/*
 * Def
 */

std::ostream& operator<<(std::ostream& os, Ref ref) { return os << *ref; }

/// This will stream @p def as an operand.
/// This is usually `id(def)` unless it can be displayed Inline.
std::ostream& operator<<(std::ostream& os, const Def* def) {
    if (def == nullptr) return os << "<nullptr>";
    if (Inline(def)) {
        auto freezer = World::Freezer(def->world());
        return os << Inline(def);
    }
    return os << id(def);
}

std::ostream& Def::stream(std::ostream& os, int max) const {
    auto freezer = World::Freezer(world());
    auto dumper  = Dumper(os);

    if (max == 0) {
        os << this << std::endl;
    } else if (auto mut = isa_decl(this)) {
        dumper.muts.push(mut);
    } else {
        dumper.recurse(this);
        dumper.tab.print(os, "{}\n", Inline(this));
        --max;
    }

    for (; !dumper.muts.empty() && max > 0; --max) dumper.dump(dumper.muts.pop());

    return os;
}

void Def::dump() const { std::cout << this << std::endl; }
void Def::dump(int max) const { stream(std::cout, max) << std::endl; }

void Def::write(int max, const char* file) const {
    auto ofs = std::ofstream(file);
    stream(ofs, max);
}

void Def::write(int max) const {
    auto file = id(this) + ".mim"s;
    write(max, file.c_str());
}

/*
 * World
 */

void World::dump(std::ostream& os) {
    auto freezer = World::Freezer(*this);
    auto old_gid = curr_gid();

    if (flags().dump_recursive) {
        auto dumper = Dumper(os);
        for (auto mut : externals()) dumper.muts.push(mut);
        while (!dumper.muts.empty()) dumper.dump(dumper.muts.pop());
    } else {
        auto nest   = Nest(*this);
        auto dumper = Dumper(os, &nest);

        for (auto name : driver().import_syms())
            print(os, ".{} {};\n", driver().is_loaded(name) ? "mim/plugin" : "import", name);
        dumper.recurse(nest.root());
    }

    assertf(old_gid == curr_gid(), "new nodes created during dump. old_gid: {}; curr_gid: {}", old_gid, curr_gid());
}

void World::dump() { dump(std::cout); }

void World::debug_dump() {
    if (log().level() == Log::Level::Debug) dump(log().ostream());
}

void World::write(const char* file) {
    auto ofs = std::ofstream(file);
    dump(ofs);
}

void World::write() {
    auto file = name().str() + ".mim"s;
    write(file.c_str());
}

} // namespace mim
