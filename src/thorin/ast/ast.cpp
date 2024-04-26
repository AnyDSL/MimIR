#include "thorin/ast/ast.h"

#include "thorin/ast/parser.h"

namespace thorin::ast {

AST::~AST() {
    assert(error().num_errors() == 0 && "please encounter any errors before destroying this class");
    if (error().num_warnings() != 0)
        driver().WLOG("{} warning(s) encountered while compiling module\n{}", error().num_warnings(), error());
}

Annex& AST::name2annex(Dbg dbg) {
    auto [plugin_s, tag_s, sub_s] = Annex::split(driver(), dbg.sym());
    auto& sym2annex               = plugin2sym2annex_[plugin_s];
    auto tag_id                   = sym2annex.size();

    if (plugin_s == sym_error()) error(dbg.loc(), "plugin name '{}' is reserved", dbg);
    if (tag_id > std::numeric_limits<tag_t>::max())
        error(dbg.loc(), "exceeded maxinum number of annexes in current plugin");

    plugin_t plugin_id;
    if (auto p = Annex::mangle(plugin_s))
        plugin_id = *p;
    else {
        error(dbg.loc(), "invalid annex name '{}'", dbg);
        plugin_s  = sym_error();
        plugin_id = *Annex::mangle(plugin_s);
    }

    if (sub_s) error(dbg.loc(), "annex '{}' must not have a subtag", dbg);

    auto [i, fresh] = sym2annex.emplace(dbg.sym(), Annex{plugin_s, tag_s, plugin_id, (tag_t)sym2annex.size()});
    auto& annex     = i->second;
    if (!fresh) annex.fresh = false;
    return annex;
}

std::pair<Annex&, bool> AST::name2annex(Sym sym, Sym plugin, Sym tag, Loc loc) {
    auto& annexes = plugin2sym2annex_[plugin];
    if (annexes.size() > std::numeric_limits<tag_t>::max())
        error(loc, "exceeded maxinum number of axioms in current plugin");

    auto plugin_id    = *Annex::mangle(plugin);
    auto [it, is_new] = annexes.emplace(sym, Annex{plugin, tag, plugin_id, (tag_t)annexes.size()});
    return {it->second, is_new};
}

void AST::bootstrap(Sym plugin, std::ostream& h) {
    Tab tab;
    tab.print(h, "#pragma once\n\n");
    tab.print(h, "#include <thorin/axiom.h>\n"
                 "#include <thorin/plugin.h>\n\n");

    tab.print(h, "/// @namespace thorin::plug::{} @ref {} \n", plugin, plugin);
    tab.print(h, "namespace thorin {{\n");
    tab.print(h, "namespace plug::{} {{\n\n", plugin);

    plugin_t plugin_id = *Annex::mangle(plugin);
    std::vector<std::ostringstream> normalizers, outer_namespace;

    tab.print(h, "static constexpr plugin_t Plugin_Id = 0x{x};\n\n", plugin_id);

    const auto& unordered = plugin2annexes(plugin);
    std::deque<std::pair<Sym, Annex>> infos(unordered.begin(), unordered.end());
    std::ranges::sort(infos, [&](const auto& p1, const auto& p2) { return p1.second.id.tag < p2.second.id.tag; });

    // clang-format off
    for (const auto& [key, annex] : infos) {
        if (annex.sym.plugin != plugin) continue; // this is from an import

        tab.print(h, "/// @name %%{}.{}\n///@{{\n", plugin, annex.sym.tag);
        tab.print(h, "#ifdef DOXYGEN // see https://github.com/doxygen/doxygen/issues/9668\n");
        tab.print(h, "enum {} : flags_t {{\n", annex.sym.tag);
        tab.print(h, "#else\n");
        tab.print(h, "enum class {} : flags_t {{\n", annex.sym.tag);
        tab.print(h, "#endif\n");
        ++tab;
        flags_t ax_id = plugin_id | (annex.id.tag << 8u);

        auto& os = outer_namespace.emplace_back();
        print(os, "template<> constexpr flags_t Annex::Base<plug::{}::{}> = 0x{x};\n", plugin, annex.sym.tag, ax_id);

        if (auto& subs = annex.subs; !subs.empty()) {
            for (const auto& aliases : subs) {
                const auto& sub = aliases.front();
                tab.print(h, "{} = 0x{x},\n", sub, ax_id++);
                for (size_t i = 1; i < aliases.size(); ++i) tab.print(h, "{} = {},\n", aliases[i], sub);

                if (annex.normalizer)
                    print(normalizers.emplace_back(), "normalizers[flags_t({}::{})] = &{}<{}::{}>;", annex.sym.tag, sub,
                          annex.normalizer, annex.sym.tag, sub);
            }
        } else {
            if (annex.normalizer)
                print(normalizers.emplace_back(), "normalizers[flags_t(Annex::Base<{}>)] = &{};", annex.sym.tag, annex.normalizer);
        }
        --tab;
        tab.print(h, "}};\n\n");

        if (!annex.subs.empty()) tab.print(h, "THORIN_ENUM_OPERATORS({})\n", annex.sym.tag);
        print(outer_namespace.emplace_back(), "template<> constexpr size_t Annex::Num<plug::{}::{}> = {};\n", plugin, annex.sym.tag, annex.subs.size());

        if (annex.normalizer) {
            if (auto& subs = annex.subs; !subs.empty()) {
                tab.print(h, "template<{}>\nRef {}(Ref, Ref, Ref);\n\n", annex.sym.tag, annex.normalizer);
            } else {
                tab.print(h, "Ref {}(Ref, Ref, Ref);\n", annex.normalizer);
            }
        }
        tab.print(h, "///@}}\n\n");
    }
    // clang-format on

    if (!normalizers.empty()) {
        tab.print(h, "void register_normalizers(Normalizers& normalizers);\n\n");
        tab.print(h, "#define THORIN_{}_NORMALIZER_IMPL \\\n", plugin);
        ++tab;
        tab.print(h, "void register_normalizers(Normalizers& normalizers) {{\\\n");
        ++tab;
        for (const auto& normalizer : normalizers) tab.print(h, "{} \\\n", normalizer.str());
        --tab;
        tab.print(h, "}}\n");
        --tab;
    }

    tab.print(h, "}} // namespace plug::{}\n\n", plugin);

    tab.print(h, "#ifndef DOXYGEN // don't include in Doxygen documentation\n");
    for (const auto& line : outer_namespace) tab.print(h, "{}", line.str());
    tab.print(h, "\n");

    // emit helpers for non-function axiom
    for (const auto& [tag, ax] : infos) {
        if (ax.pi || ax.sym.plugin != plugin) continue; // from function or other plugin?
        tab.print(h, "template<> struct Axiom::Match<plug::{}::{}> {{ using type = Axiom; }};\n", ax.sym.plugin,
                  ax.sym.tag);
    }

    tab.print(h, "#endif\n");
    tab.print(h, "}} // namespace thorin\n");
}

/*
 * Other
 */

LamExpr::LamExpr(Ptr<LamDecl>&& lam)
    : Expr(lam->loc())
    , lam_(std::move(lam)) {}

/*
 * Ptrn::to_expr/to_ptrn
 */

Ptr<Expr> Ptrn::to_expr(AST& ast, Ptr<Ptrn>&& ptrn) {
    if (auto idp = ptrn->isa<IdPtrn>(); idp && !idp->dbg() && idp->type()) {
        if (auto ide = idp->type()->isa<IdExpr>()) return ast.ptr<IdExpr>(ide->dbg());
    } else if (auto tuple = ptrn->isa<TuplePtrn>(); tuple && tuple->is_brckt()) {
        (void)ptrn.release();
        return ast.ptr<SigmaExpr>(Ptr<TuplePtrn>(tuple));
    }
    return {};
}

Ptr<Ptrn> Ptrn::to_ptrn(Ptr<Expr>&& expr) {
    if (auto sigma = expr->isa<SigmaExpr>())
        return std::move(const_cast<SigmaExpr*>(sigma)->ptrn_); // TODO get rid off const_cast
    return {};
}

void Module::compile(AST& ast) const {
    bind(ast);
    ast.error().ack();
    emit(ast);
}

AST load_plugins(World& world, View<Sym> plugins) {
    auto tag = Tok::Tag::K_import;
    if (!world.driver().flags().bootstrap) {
        for (auto plugin : plugins) world.driver().load(plugin);
        tag = Tok::Tag::K_plugin;
    }

    auto ast     = AST(world);
    auto parser  = Parser(ast);
    auto imports = Ptrs<Import>();

    for (auto plugin : plugins)
        if (auto mod = parser.import(plugin, nullptr))
            imports.emplace_back(ast.ptr<Import>(mod->loc(), tag, Dbg(plugin), std::move(mod)));

    if (!plugins.empty()) {
        auto mod = ast.ptr<Module>(imports.front()->loc() + imports.back()->loc(), std::move(imports), Ptrs<ValDecl>());
        mod->compile(ast);
    }

    return ast;
}

} // namespace thorin::ast
