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
        if (auto mut = def_->isa_mut()) {
            if (isa_decl(mut)) return false;
            return true;
        }

        if (def_->is_closed()) return true;

        if (auto app = def_->isa<App>()) {
            if (app->type()->isa<Pi>()) return true; // curried apps are printed inline
            if (app->type()->isa<Type>()) return true;
            if (app->callee()->isa<Axm>()) return app->callee_type()->num_doms() <= 1;
            return false;
        }

        return true;
    }

private:
    const Def* def_;
    const int dump_gid_;

    friend std::ostream& operator<<(std::ostream&, Inline);
};

using ast::Assoc;
using ast::Prec;
using ast::prec_assoc;

bool is_atomic_app(const Def* def) {
    if (auto app = def->isa<App>()) {
        if (auto size = Idx::isa(app)) {
            if (auto l = Lit::isa(size)) {
                switch (*l) {
                        // clang-format off
                    case 0x0'0000'0002_n:
                    case 0x0'0000'0100_n:
                    case 0x0'0001'0000_n:
                    case 0x1'0000'0000_n:
                    case             0_n: return true;
                        // clang-format on
                    default: break;
                }
            }
        }
    }
    return false;
}

Prec def2prec(const Def* def) {
    if (def->isa<Extract>()) return Prec::Extract;
    if (auto pi = def->isa<Pi>(); pi && !Pi::isa_cn(pi)) return Prec::Arrow;
    if (def->isa<App>() && !is_atomic_app(def)) return Prec::App;
    return Prec::Lit;
}

bool needs_parens(Prec parent, const Def* child, bool is_left) {
    if (!Inline(child)) return false;

    auto child_prec = def2prec(child);
    if (child_prec < parent) return true;
    if (child_prec > parent) return false;

    switch (prec_assoc(parent)) {
        case Assoc::Right: return is_left;
        case Assoc::Left:  return !is_left;
        case Assoc::None:  return false;
    }
    fe::unreachable();
}

std::ostream& dump_child(std::ostream& os, Prec parent, const Def* child, bool is_left) {
    if (needs_parens(parent, child, is_left)) return print(os, "({})", child);
    return print(os, "{}", child);
}

std::ostream& dump_ascribed(std::ostream& os, const auto& value, const Def* type) {
    os << value << ':';
    if (def2prec(type) == Prec::Lit) return print(os, "{}", type);
    return print(os, "({})", type);
}

std::ostream& dump_pi_dom(std::ostream& os, const Pi* pi) {
    auto mut = pi->isa_mut<Pi>();
    auto l   = pi->is_implicit() ? '{' : '[';
    auto r   = pi->is_implicit() ? '}' : ']';

    if (mut && mut->var()) {
        os << l << mut->var() << ": ";
        dump_child(os, Prec::Arrow, pi->dom(), true);
        return os << r;
    }

    return dump_child(os, Prec::Arrow, pi->dom(), true);
}

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
            if (level == 0) return print(os, "*");
            if (level == 1) return print(os, "□");
        }
        os << "Type ";
        return dump_child(os, Prec::App, type->level(), false);
    } else if (u->isa<Nat>()) {
        return print(os, "Nat");
    } else if (u->isa<Idx>()) {
        return print(os, "Idx");
    } else if (auto ext = u->isa<Ext>()) {
        auto x = ext->isa<Bot>() ? (ascii ? "bot" : "⊥") : (ascii ? "top" : "⊤");
        return dump_ascribed(os, x, ext->type());
    } else if (auto top = u->isa<Top>()) {
        return dump_ascribed(os, ascii ? "top" : "⊤", top->type());
    } else if (auto axm = u->isa<Axm>()) {
        const auto name = axm->sym();
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
        return dump_ascribed(os, lit->get(), lit->type());
    } else if (auto ex = u->isa<Extract>()) {
        if (ex->tuple()->isa<Var>() && ex->index()->isa<Lit>()) return print(os, "{}", ex->unique_name());
        dump_child(os, Prec::Extract, ex->tuple(), true);
        os << '#';
        return dump_child(os, Prec::Extract, ex->index(), false);
    } else if (auto var = u->isa<Var>()) {
        return print(os, "{}", var->unique_name());
    } else if (auto pi = u->isa<Pi>()) {
        if (Pi::isa_cn(pi)) {
            os << "Cn ";
            return dump_pi_dom(os, pi);
        }
        dump_pi_dom(os, pi);
        os << ' ' << arw << ' ';
        return dump_child(os, Prec::Arrow, pi->codom(), false);
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
        dump_child(os, Prec::App, app->callee(), true);
        os << ' ';
        return dump_child(os, Prec::App, app->arg(), false);
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
            return print(os, "{}{}: {}; {}{}", al, mut->var(), mut->arity(), mut->body(), ar);
        return print(os, "{}{}; {}{}", al, arr->arity(), arr->body(), ar);
    } else if (auto pack = u->isa<Pack>()) {
        if (auto mut = pack->isa_mut<Pack>(); mut && mut->var())
            return print(os, "{}{}: {}; {}{}", pl, mut->var(), mut->arity(), mut->body(), pr);
        return print(os, "{}{}; {}{}", pl, pack->arity(), pack->body(), pr);
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
    void dump_group(const Def*, const Def*, bool implicit, bool paren_style, size_t limit, bool alias = true);
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
        if (def->isa<Arr>()) return "Arr";
        if (def->isa<Pack>()) return "pack";
        if (def->isa<Pi>()) return "Pi";
        if (def->isa<Hole>()) return "Hole";
        if (def->isa<Rule>()) return "Rule";
        fe::unreachable();
    };

    auto mut_op0 = [&](const Def* def) -> std::ostream& {
        if (auto sig = def->isa<Sigma>()) return print(os, ", {}", sig->num_ops());
        if (auto arr = def->isa<Arr>()) return print(os, ", {}", arr->arity());
        if (auto pack = def->isa<Pack>()) return print(os, ", {}", pack->arity());
        if (auto pi = def->isa<Pi>()) return print(os, ", {}", pi->dom());
        if (auto hole = def->isa_mut<Hole>()) return hole->is_set() ? print(os, ", {}", hole->op()) : print(os, ", ??");
        if (auto rule = def->isa<Rule>()) return print(os, "{} => {}", rule->lhs(), rule->rhs());
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
    if (nest) recurse((*nest)[mut]);
    recurse(mut);
    tab.print(os, "{, }\n", mut->ops());
    --tab;
    tab.print(os, "}};\n");
}

void Dumper::dump(Lam* lam) {
    std::vector<Lam*> groups;
    for (Lam* curr = lam;;) {
        groups.emplace_back(curr);
        auto body = curr->body();
        if (!body) break;
        auto next = body->isa<Lam>();
        if (!next) break;
        curr = const_cast<Lam*>(next);
    }

    auto last   = groups.back();
    auto is_fun = Lam::isa_returning(last);
    auto is_con = Lam::isa_cn(last) && !is_fun;

    tab.print(os, "{} {}{}", is_fun ? "fun" : is_con ? "con" : "lam", external(lam), id(lam));
    for (auto* group : groups) {
        os << ' ';
        auto num_doms = group->var() ? group->var()->num_tprojs() : group->type()->dom()->num_tprojs();
        auto limit    = is_fun && group == last ? num_doms - 1 : num_doms;
        dump_group(group->var(),
                   group->type()->dom(),
                   group->type()->is_implicit(),
                   !is_con,
                   limit,
                   !is_fun || group != last);
        if (is_con && group == last) print(os, "@({})", group->filter());
    }

    if (is_fun)
        print(os, ": {} =", last->ret_dom());
    else if (!is_con)
        print(os, ": {} =", last->type()->codom());
    else
        print(os, " =");
    os << '\n';

    ++tab;
    if (last->is_set()) {
        if (nest && groups.size() == 1) recurse((*nest)[lam]);
        for (auto* group : groups)
            recurse(group->filter());
        recurse(last->body(), true);
        if (last->body()->isa_mut())
            tab.print(os, "{};\n", last->body());
        else
            tab.print(os, "{};\n", Inline(last->body()));
    } else {
        tab.print(os, "<unset>;\n");
    }
    --tab;
    tab.print(os, "\n");
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
            print(os,
                  "({, }) as {}",
                  Elem(projs, [&](auto proj) { dump_ptrn(proj, type->proj(i++)); }),
                  def->unique_name());
        }
    }
}

void Dumper::dump_group(const Def* def, const Def* type, bool implicit, bool paren_style, size_t limit, bool alias) {
    auto l = implicit ? '{' : paren_style ? '(' : '[';
    auto r = implicit ? '}' : paren_style ? ')' : ']';
    auto dump_binder = [&](const Def* binder, const Def* binder_type) {
        if (binder)
            dump_ptrn(binder, binder_type);
        else
            print(os, "_: {}", binder_type);
    };

    if (limit == 0) {
        os << l << r;
        return;
    }

    if (limit == 1) {
        os << l;
        dump_binder(def ? def->tproj(0) : nullptr, type->tproj(0));
        os << r;
        return;
    }

    os << l;
    print(os,
          "{, }",
          Elem(std::views::iota(size_t(0), limit), [&](auto i) { dump_binder(def ? def->tproj(i) : nullptr, type->tproj(i)); }));
    os << r;
    if (alias && def) print(os, " as {}", def->unique_name());
}

void Dumper::recurse(const Nest::Node* node) {
    for (auto child : node->children().muts())
        if (auto mut = isa_decl(child)) dump(mut);
}

void Dumper::recurse(const Def* def, bool first /*= false*/) {
    if (auto mut = isa_decl(def)) {
        if (!nest) muts.push(mut);
        return;
    }

    if (!defs.emplace(def).second) return;

    for (auto op : def->deps())
        recurse(op);

    if (!first && !Inline(def)) dump_let(def);
}

} // namespace

/*
 * Def
 */

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

    for (; !dumper.muts.empty() && max > 0; --max)
        dumper.dump(dumper.muts.pop());

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
        for (auto mut : externals().muts())
            dumper.muts.push(mut);
        while (!dumper.muts.empty())
            dumper.dump(dumper.muts.pop());
    } else {
        auto nest   = Nest(*this);
        auto dumper = Dumper(os, &nest);

        for (const auto& import : driver().imports())
            print(os,
                  "{} {};\n",
                  import.tag == ast::Tok::Tag::K_plugin ? "plugin" : "import",
                  import.sym);
        dumper.recurse(nest.root());
    }

    assertf(old_gid == curr_gid(), "new nodes created during dump. old_gid: {}; curr_gid: {}", old_gid, curr_gid());
}

void World::dump() { dump(std::cout); }

void World::debug_dump() {
    if (log().level() >= Log::Level::Debug) dump(log().ostream());
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
