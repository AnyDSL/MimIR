#include "thorin/be/h/h.h"

#include "thorin/util/print.h"

namespace thorin::h {

void Bootstrapper::emit(std::ostream& h) {
    tab.print(h, "#ifndef THORIN_{}_H\n", dialect_);
    tab.print(h, "#define THORIN_{}_H\n\n", dialect_);
    tab.print(h, "namespace thorin::{} {{\n\n", dialect_);

    tab.print(h, "enum Tag : tag_t {{\n");
    ++tab;
    for (const auto& ax : axioms) tab.print(h, "{},\n", ax.group);
    --tab;
    tab.print(h, "}}\n\n");

    for (const auto& ax : axioms) {
        if (auto& tags = ax.tags; !tags.empty()) {
            tab.print(h, "enum class {} : flags_t {{\n", ax.group);
            ++tab;
            for (const auto& aliases : tags) {
                const auto& tag = aliases.front();
                tab.print(h, "{},\n", tag);
                for (size_t i = 1; i < aliases.size(); ++i) tab.print(h, "{} = {},\n", aliases[i], tag);
            }
            --tab;
            tab.print(h, "}}\n\n");
        }
    }

    tab.print(h, "}} // namespace thorin::{}\n", dialect_);
}

} // namespace thorin::h
