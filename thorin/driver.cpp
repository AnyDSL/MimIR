#include "thorin/driver.h"
#include <filesystem>

#include "thorin/util/dl.h"
#include "thorin/util/sys.h"

namespace thorin {

static std::vector<fs::path> get_plugin_name_variants(std::string_view name) {
    std::vector<fs::path> names;
    names.push_back(name); // if the user gives "libthorin_foo.so"
    names.push_back(fmt("{}thorin_{}{}", dl::prefix(), name, dl::extension()));
    return names;
}

Driver::Driver()
    : world_(this) {
    // paths from env
    if (auto env_path = std::getenv("THORIN_DIALECT_PATH")) {
        std::stringstream env_path_stream{env_path};
        std::string sub_path;
        while (std::getline(env_path_stream, sub_path, ':')) add_search_path(sub_path);
    }

    // add path/to/thorin.exe/../../lib/thorin
    if (auto path = sys::path_to_curr_exe()) add_search_path(path->parent_path().parent_path() / "lib" / "thorin");

    // add install path if different from above
    if (auto install_path = fs::path{THORIN_INSTALL_PREFIX} / "lib" / "thorin"; fs::exists(install_path)) {
        if (search_paths().empty() || !fs::equivalent(install_path, search_paths().back()))
            add_search_path(std::move(install_path));
    }

    // all other user paths take precedence over the fallbacks above
    insert_ = search_paths_.begin();
}

void Driver::load(Sym name) {
    ILOG("loading plugin: '{}'", name);

    if (plugin(name)) {
        WLOG("plugin '{}' already loaded", name);
        return;
    }

    Dialect::Handle handle{nullptr, dl::close};
    auto plugin_path = *name;
    if (auto path = fs::path{*name}; path.is_absolute() && fs::is_regular_file(path)) handle.reset(dl::open(*name));
    if (!handle) {
        for (const auto& path : search_paths()) {
            for (auto name_variants = get_plugin_name_variants(name); const auto& name_variant : name_variants) {
                auto full_path = path / name_variant;
                plugin_path    = full_path.string();

                std::error_code ignore;
                if (bool reg_file = fs::is_regular_file(full_path, ignore); reg_file && !ignore) {
                    auto path_str = full_path.string();
                    if (handle.reset(dl::open(path_str)); handle) break;
                }
            }
            if (handle) break;
        }
    }

    if (!handle) err("cannot open plugin '{}'", name);

    auto& [_, plugin] = *assert_emplace(plugins_, name, Dialect{plugin_path, std::move(handle)});
    plugin.register_passes(passes_);
    plugin.register_backends(backends_);
    plugin.register_normalizers(normalizers_);
}

const fs::path* Driver::add_import(fs::path path, Sym sym) {
    for (auto& [p, _] : imports_) {
        if (fs::equivalent(path, p)) return nullptr;
    }

    imports_.emplace_back(std::pair(std::move(path), sym));
    return &imports_.back().first;
}

} // namespace thorin
