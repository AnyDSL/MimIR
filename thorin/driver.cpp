#include "thorin/driver.h"

#include "thorin/util/dl.h"

namespace thorin {

static std::vector<fs::path> get_plugin_name_variants(std::string_view name) {
    std::vector<fs::path> names;
    names.push_back(name); // if the user gives "libthorin_foo.so"
    names.push_back(fmt("{}thorin_{}{}", dl::prefix(), name, dl::extension()));
    return names;
}

Driver::Driver()
    : log_(*this)
    , world_(this) {
    add_search_path(fs::current_path());

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

    // all other user paths take precedence and are inserted before above fallbacks
    insert_ = search_paths_.begin();
}

Dialect Driver::load(const std::string& name) {
    std::unique_ptr<void, decltype(&dl::close)> handle{nullptr, dl::close};
    auto plugin_path = name;
    if (auto path = fs::path{name}; path.is_absolute() && fs::is_regular_file(path)) handle.reset(dl::open(name));
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

    if (!handle) throw std::runtime_error("cannot open plugin '" + name + "'");

    auto dialect = Dialect{plugin_path, std::move(handle)};
    dialect.register_normalizers(normalizers_);
    dialect.register_backends(backends_);
    return dialect;
}

} // namespace thorin
