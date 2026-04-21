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

using ast::Assoc;
using ast::Prec;
using ast::prec_assoc;

/// This is a wrapper to dump a Def.
class Op {
public:
    Op(const Def* def, Prec prec = Prec::Bot, bool is_left = false)
        : def_(def)
        , prec_(prec)
        , is_left_(is_left) {}

    /// @name Getters
    ///@{
    const Def* def() const { return def_; }
    const Def* operator->() const { return def_; }
    const Def* operator*() const { return def_; }
    Prec prec() const { return prec_; }
    bool is_left() const { return is_left_; }
    ///@}

private:
    const Def* def_;
    Prec prec_;
    bool is_left_;

    /// This will stream @p def as an operand.
    /// This is usually `id(def)` unless it can be displayed Inline.
    friend std::ostream& operator<<(std::ostream&, Op);
};

/// This is a wrapper to dump a Def.
class Dump : public Op {
public:
    Dump(const Def* def, Prec prec = Prec::Bot, bool is_left = false)
        : Op(def, prec, is_left) {}
    Dump(Op op)
        : Dump(op.def(), op.prec(), op.is_left()) {}

    explicit operator bool() const {
        if (auto mut = def()->isa_mut()) {
            if (isa_decl(mut)) return false;
            return true;
        }

        if (def()->is_closed()) return true;

        if (auto app = def()->isa<App>()) {
            if (app->type()->isa<Pi>()) return true; // curried apps are printed inline
            if (app->type()->isa<Type>()) return true;
            if (app->callee()->isa<Axm>()) return app->callee_type()->num_doms() <= 1;
            return false;
        }

        return true;
    }

    friend std::ostream& operator<<(std::ostream&, Dump);
};

auto ops(std::ostream& os, Defs defs) {
    return Elem(defs, [&os](const auto& e) { os << Op(e); });
}

bool is_atomic_app(const Def* def) {
    if (auto app = def->isa<App>()) {
        if (auto size = Idx::isa(app)) {
            if (auto l = Lit::isa(size)) {
                // clang-format off
                switch (*l) {
                    case 0x0'0000'0002_n:
                    case 0x0'0000'0100_n:
                    case 0x0'0001'0000_n:
                    case 0x1'0000'0000_n:
                    case             0_n: return true;
                    default: break;
                }
                // clang-format on
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
    if (!Dump(child)) return false;

    auto child_prec = def2prec(child);
    if (child_prec < parent) return true;
    if (child_prec > parent) return false;

    switch (prec_assoc(parent)) {
        case Assoc::R: return is_left;
        case Assoc::L: return !is_left;
        case Assoc::N: return false;
    }
    fe::unreachable();
}

/// This will stream @p def as an operand.
/// This is usually `id(def)` unless it can be inlined.
std::ostream& operator<<(std::ostream& os, Op op) {
    if (*op == nullptr) return os << "<nullptr>";
    if (auto d = Dump(op)) return os << d;
    return os << id(*op);
}

std::ostream& operator<<(std::ostream& os, Dump d) {
    if (auto mut = d->isa_mut(); mut && !mut->is_set()) return os << "unset";
    if (needs_parens(d.prec(), *d, d.is_left())) return print(os, "({})", Dump(*d));

    bool ascii = d->world().flags().ascii;
    auto arw   = ascii ? "->" : "→";
    auto al    = ascii ? "<<" : "«";
    auto ar    = ascii ? ">>" : "»";
    auto pl    = ascii ? "(<" : "‹";
    auto pr    = ascii ? ">)" : "›";
    auto bot   = ascii ? "bot" : "⊥";
    auto top   = ascii ? "top" : "⊤";

    if (auto type = d->isa<Type>()) {
        if (auto level = Lit::isa(type->level()); level && !ascii) {
            if (level == 0) return print(os, "*");
            if (level == 1) return print(os, "□");
        }
        return print(os, "Type {}", Op(type->level(), Prec::App, false));
    } else if (d->isa<Nat>()) {
        return print(os, "Nat");
    } else if (d->isa<Idx>()) {
        return print(os, "Idx");
    } else if (auto ext = d->isa<Ext>()) {
        return print(os, "{}:{}", ext->isa<Bot>() ? bot : top, Op(ext->type(), Prec::Lit, false));
    } else if (auto axm = d->isa<Axm>()) {
        const auto name = axm->sym();
        return print(os, "{}{}", name[0] == '%' ? "" : "%", name);
    } else if (auto lit = d->isa<Lit>()) {
        if (lit->type()->isa<Nat>()) {
            // clang-format off
            switch (lit->get()) {
                case 0x0'0000'0100_n: return os << "i8";
                case 0x0'0001'0000_n: return os << "i16";
                case 0x1'0000'0000_n: return os << "i32";
                default: return print(os, "{}", lit->get());
            }
            // clang-format on
        } else if (auto size = Idx::isa(lit->type())) {
            if (auto s = Lit::isa(size)) {
                // clang-format off
                switch (*s) {
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
                }
                // clang-format on
            }
        }
        return print(os, "{}:{}", lit->get(), Op(lit->type(), Prec::Lit, false));
    } else if (auto ex = d->isa<Extract>()) {
        if (ex->tuple()->isa<Var>() && ex->index()->isa<Lit>()) return os << ex->unique_name();
        return print(os, "{}#{}", Op(ex->tuple(), Prec::Extract, true), Dump(ex->index(), Prec::Extract, false));
    } else if (auto var = d->isa<Var>()) {
        return os << var->unique_name();
    } else if (auto pi = d->isa<Pi>()) {
        if (Pi::isa_cn(pi)) return print(os, "Cn {}", Op(pi->dom()));

        if (auto mut = pi->isa_mut<Pi>()) {
            if (auto var = mut->has_var()) {
                auto l = pi->is_implicit() ? '{' : '[';
                auto r = pi->is_implicit() ? '}' : ']';
                return print(os, "{}{}: {}{} {} {}", l, Op(var), Op(pi->dom()), r, arw,
                             Op(pi->dom(), Prec::Arrow, false));
            }
        }

        return print(os, "{} {} {}", Op(pi->dom(), Prec::Arrow, true), arw, Op(pi->dom(), Prec::Arrow, false));
    } else if (auto lam = d->isa<Lam>()) {
        // TODO this output is really confuinsg
        return print(os, "{}, {}", Op(lam->filter()), Op(lam->body()));
    } else if (auto app = d->isa<App>()) {
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

        return print(os, "{} {}", Op(app->callee(), Prec::App, true), Op(app->arg(), Prec::App, false));
    } else if (auto sigma = d->isa<Sigma>()) {
        if (auto mut = sigma->isa_mut<Sigma>(); mut && mut->var()) {
            size_t i = 0;
            return print(os, "[{, }]", Elem(sigma->ops(), [&](auto op) {
                             if (auto v = mut->var(i++))
                                 print(os, "{}: {}", v, Op(op));
                             else
                                 os << Op(op);
                         }));
        }
        return print(os, "[{, }]", ops(os, sigma->ops()));
    } else if (auto tuple = d->isa<Tuple>()) {
        return print(os, "({, })", ops(os, tuple->ops()));
    } else if (auto arr = d->isa<Arr>()) {
        if (auto mut = arr->isa_mut<Arr>(); mut && mut->var())
            return print(os, "{}{}: {}; {}{}", al, mut->var(), Op(mut->arity()), Op(mut->body()), ar);
        return print(os, "{}{}; {}{}", al, Op(arr->arity()), Op(arr->body()), ar);
    } else if (auto pack = d->isa<Pack>()) {
        if (auto mut = pack->isa_mut<Pack>(); mut && mut->var())
            return print(os, "{}{}: {}; {}{}", pl, mut->var(), Op(mut->arity()), Op(mut->body()), pr);
        return print(os, "{}{}; {}{}", pl, Op(pack->arity()), Op(pack->body()), pr);
    } else if (auto proxy = d->isa<Proxy>()) {
        return print(os, ".proxy#{}#{} {, }", proxy->pass(), proxy->tag(), ops(os, proxy->ops()));
    } else if (auto bound = d->isa<Bound>()) {
        auto op = bound->isa<Join>() ? "∪" : "∩"; // TODO ascii
        if (auto mut = d->isa_mut()) print(os, "{}{}: {}", op, mut->unique_name(), Op(mut->type()));
        return print(os, "{}({, })", op, ops(os, bound->ops()));
    }

    // other
    if (d->flags() == 0) return print(os, "(.{} {, })", d->node_name(), d->ops());
    return print(os, "(.{}#{} {, })", d->node_name(), d->flags(), ops(os, d->ops()));
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
        dump_group(group->var(), group->type()->dom(), group->type()->is_implicit(), !is_con, limit,
                   !is_fun || group != last);
        if (is_con && group == last) print(os, "@{}", Op(group->filter()));
    }

    if (is_fun)
        print(os, ": {} =", Op(last->ret_dom()));
    else if (!is_con)
        print(os, ": {} =", Op(last->type()->codom()));
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
            tab.print(os, "{};\n", Dump(last->body()));
    } else {
        tab.print(os, "<unset>;\n");
    }
    --tab;
    tab.print(os, "\n");
}

void Dumper::dump_let(const Def* def) {
    tab.print(os, "let {}: {} = {};\n", def->unique_name(), Op(def->type()), Dump(def));
}

void Dumper::dump_ptrn(const Def* def, const Def* type) {
    if (!def) {
        os << Op(type);
    } else {
        auto projs = def->tprojs();
        if (projs.size() == 1 || std::ranges::all_of(projs, [](auto def) { return !def; })) {
            print(os, "{}: {}", def->unique_name(), Op(type));
        } else {
            size_t i = 0;
            print(os, "({, }) as {}", Elem(projs, [&](auto proj) { dump_ptrn(proj, type->proj(i++)); }),
                  def->unique_name());
        }
    }
}

void Dumper::dump_group(const Def* def, const Def* type, bool implicit, bool paren_style, size_t limit, bool alias) {
    auto l           = implicit ? '{' : paren_style ? '(' : '[';
    auto r           = implicit ? '}' : paren_style ? ')' : ']';
    auto dump_binder = [&](const Def* binder, const Def* binder_type) {
        if (binder)
            dump_ptrn(binder, binder_type);
        else
            print(os, "_: {}", Op(binder_type));
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

    print(os, "{}{, }{}", l,
          Elem(std::views::iota(size_t(0), limit),
               [&](auto i) { dump_binder(def ? def->tproj(i) : nullptr, type->tproj(i)); }),
          r);

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

    if (!first && !Dump(def)) dump_let(def);
}

} // namespace

/*
 * Def
 */

/// This will stream @p def as an operand.
/// This is usually `id(def)` unless it can be displayed Inline.
std::ostream& operator<<(std::ostream& os, const Def* def) {
    auto _ = def->world().freeze();
    return println(os, "let {}: {} = {}", def->unique_name(), Op(def->type()), Dump(def));
}

void Def::dump() const { std::cout << this << std::endl; }

std::string Def::to_string() const {
    std::ostringstream os;
    os << this;
    return os.str();
}

/*
 * World
 */

void World::dump(std::ostream& os) {
    auto _       = freeze();
    auto old_gid = curr_gid();
    auto nest    = Nest(*this);
    auto dumper  = Dumper(os, &nest);

    for (const auto& import : driver().imports())
        print(os, "{} {};\n", import.tag == ast::Tok::Tag::K_plugin ? "plugin" : "import", import.sym);
    dumper.recurse(nest.root());

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
