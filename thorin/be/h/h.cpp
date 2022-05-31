#include "thorin/be/h/h.h"

#include <sstream>

#include "thorin/axiom.h"
#include "thorin/error.h"

#include "thorin/util/print.h"

namespace thorin::h {

void Bootstrapper::emit(std::ostream& h) {
    tab.print(h, "#ifndef THORIN_{}_H\n", dialect_);
    tab.print(h, "#define THORIN_{}_H\n\n", dialect_);
    tab.print(h, "#include \"thorin/tables.h\"\n"
                 "#include \"thorin/axiom.h\"\n\n");

    tab.print(h, "namespace thorin::{} {{\n\n", dialect_);

    tab.print(h, "enum Tag : u64 {{\n");
    ++tab;
    h << std::hex;
    // for (const auto& ax : axioms) { tab.print(h, "{}_{} = 0x{},\n", ax.dialect, ax.group, ax.id); }
    h << std::dec;
    --tab;
    tab.print(h, "}};\n\n");

    tab.print(h, "template<fields_t tag>\n"
                 "struct Tag2Def_ {{\n"
                 "    using type = App;\n"
                 "}};\n\n");

    for (const auto& ax : axioms)
        if (!ax.pi)
            tab.print(h,
                      "template<>\n"
                      "struct Tag2Def_<Tag::{}_{}> {{"
                      "    using type = Axiom;"
                      "}};",
                      ax.dialect, ax.group);

    tab.print(h, "template<fields_t tag>\n"
                 "using Tag2Def = typename Tag2Def_<tag>::type;\n\n"

                 "template<fields_t tag>\n"
                 "struct Tag2Enum_ {{ using type = fields_t; }};\n\n");

    for (const auto& ax : axioms) {
        if (auto& tags = ax.tags; !tags.empty()) {
            tab.print(h, "enum class {} : u8 {{\n", ax.group);
            ++tab;
            for (const auto& aliases : tags) {
                const auto& tag = aliases.front();
                tab.print(h, "{},\n", tag);
                for (size_t i = 1; i < aliases.size(); ++i) tab.print(h, "{} = {},\n", aliases[i], tag);
            }
            --tab;
            tab.print(h, "}};\n\n");

            tab.print(h,
                      "template<>\n"
                      "struct Tag2Enum_<Tag::{}_{}> {{ using type = {}; }};\n",
                      ax.dialect, ax.group, ax.group);
        }
    }

    tab.print(
        h,
        "template<fields_t tag> using Tag2Enum = typename Tag2Enum_<tag>::type;\n\n"

        "template<fields_t tag>\n"
        "Query<Tag2Enum<tag>, Tag2Def<tag>> isa(const Def* def) {{\n"
        "    auto [axiom, curry] = Axiom::get(def);\n"
        "    if (axiom && axiom->fields() >> 8u == tag >> 8u && curry == 0) return {{axiom, "
        "def->as<Tag2Def<tag>>()}};\n"
        "    return {{}};\n"
        "}}\n\n"

        "template<fields_t tag>\n"
        "Query<Tag2Enum<tag>, Tag2Def<tag>> isa(Tag2Enum<tag> flags, const Def* def) {{\n"
        "    auto [axiom, curry] = Axiom::get(def);\n"
        "    if (axiom && axiom->fields() >> 8u == tag >> 8u && (axiom->flags() & 0xFF) == flags_t(flags) && curry == "
        "0)\n"
        "        return {{axiom, def->as<Tag2Def<tag>>()}};\n"
        "    return {{}};\n"
        "}}\n\n"

        "template<fields_t t>\n"
        "Query<Tag2Enum<t>, Tag2Def<t>> as(const Def* d) {{\n"
        "    assert(isa<t>(d));\n"
        "    return {{std::get<0>(Axiom::get(d)), d->as<App>()}};\n"
        "}}\n\n"

        "template<fields_t t>\n"
        "Query<Tag2Enum<t>, Tag2Def<t>> as(Tag2Enum<t> f, const Def* d) {{\n"
        "    assert((isa<t>(f, d)));\n"
        "    return {{std::get<0>(Axiom::get(d)), d->as<App>()}};\n"
        "}}\n");

    tab.print(h, "}} // namespace thorin::{}\n\n", dialect_);
    tab.print(h, "#endif\n");
}

} // namespace thorin::h
