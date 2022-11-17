#include "thorin/be/h/bootstrapper.h"

#include <ranges>
#include <sstream>

#include "thorin/axiom.h"
#include "thorin/error.h"

namespace thorin::h {

void Bootstrapper::emit(std::ostream& h) {
    tab.print(h, "#pragma once\n\n");
    tab.print(h, "#include \"thorin/axiom.h\"\n"
                 "#include \"thorin/dialects.h\"\n\n");

    tab.print(h, "namespace thorin {{\nnamespace {} {{\n\n", dialect_);

    dialect_t dialect_id = *Axiom::mangle(dialect_);
    std::vector<std::ostringstream> normalizers, outer_namespace;

    tab.print(h << std::hex, "static constexpr dialect_t Dialect_Id = 0x{};\n\n", dialect_id);

    // clang-format off
    for (const auto& [key, ax] : axioms) {
        tab.print(h, "enum class {} : flags_t {{\n", ax.tag);
        ++tab;
        flags_t ax_id = dialect_id | (ax.tag_id << 8u);

        auto& os = outer_namespace.emplace_back();
        print(os << std::hex, "template<> constexpr flags_t Axiom::Base<{}::{}> = 0x{};\n", dialect_, ax.tag, ax_id);

        if (auto& subs = ax.subs; !subs.empty()) {
            for (const auto& aliases : subs) {
                const auto& sub = aliases.front();
                tab.print(h, "{} = 0x{},\n", sub, ax_id++);
                for (size_t i = 1; i < aliases.size(); ++i) tab.print(h, "{} = {},\n", aliases[i], sub);

                if (!ax.normalizer.empty())
                    print(normalizers.emplace_back(), "normalizers[flags_t({}::{})] = &{}<{}::{}>;", ax.tag, sub,
                          ax.normalizer, ax.tag, sub);
            }
        } else {
            if (!ax.normalizer.empty())
                print(normalizers.emplace_back(), "normalizers[flags_t(Axiom::Base<{}>)] = &{};", ax.tag, ax.normalizer);
        }
        --tab;
        tab.print(h, "}};\n\n");

        if (!ax.subs.empty()) {
            tab.print(h, "inline bool operator==({} lhs, flags_t rhs) {{ return static_cast<flags_t>(lhs) == rhs; }}\n", ax.tag);
            tab.print(h, "inline flags_t operator&({} lhs, flags_t rhs) {{ return static_cast<flags_t>(lhs) & rhs; }}\n", ax.tag);
            tab.print(h, "inline flags_t operator&({} lhs, {} rhs) {{ return static_cast<flags_t>(lhs) & static_cast<flags_t>(rhs); }}\n", ax.tag, ax.tag);
            tab.print(h, "inline flags_t operator|({} lhs, flags_t rhs) {{ return static_cast<flags_t>(lhs) | rhs; }}\n", ax.tag);
            tab.print(h, "inline flags_t operator|({} lhs, {} rhs) {{ return static_cast<flags_t>(lhs) | static_cast<flags_t>(rhs); }}\n\n", ax.tag, ax.tag);
        }

        print(outer_namespace.emplace_back(), "template<> constexpr size_t Axiom::Num<{}::{}> = {};\n", dialect_, ax.tag, ax.subs.size());

        if (!ax.normalizer.empty()) {
            if (auto& subs = ax.subs; !subs.empty()) {
                tab.print(h, "template<{}>\nconst Def* {}(const Def*, const Def*, const Def*, const Def*);\n\n", ax.tag, ax.normalizer);
            } else {
                tab.print(h, "const Def* {}(const Def*, const Def*, const Def*, const Def*);\n\n", ax.normalizer);
            }
        }
    }
    // clang-format on

    if (!normalizers.empty()) {
        tab.print(h, "void register_normalizers(Normalizers& normalizers);\n\n");
        tab.print(h, "#define THORIN_{}_NORMALIZER_IMPL \\\n", dialect_);
        ++tab;
        tab.print(h, "void register_normalizers(Normalizers& normalizers) {{\\\n");
        ++tab;
        for (const auto& normalizer : normalizers) tab.print(h, "{} \\\n", normalizer.str());
        --tab;
        tab.print(h, "}}\n");
        --tab;
    }

    tab.print(h, "}} // namespace {}\n\n", dialect_);

    for (const auto& line : outer_namespace) { tab.print(h, "{}", line.str()); }
    tab.print(h, "\n");

    if (std::ranges::any_of(axioms, [](const auto& ax) { return !ax.second.pi; })) {
        for (const auto& [tag, ax] : axioms) {
            if (ax.pi) continue;
            tab.print(h, "template<> struct Axiom::Match<{}::{}> {{ using type = Axiom; }};\n", ax.dialect, ax.tag);
        }
    }

    tab.print(h, "}} // namespace thorin\n");
}

} // namespace thorin::h
