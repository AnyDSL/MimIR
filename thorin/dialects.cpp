#include "thorin/dialects.h"

#include <cstdlib>

#include <memory>
#include <sstream>
#include <string>

#include "thorin/config.h"
#include "thorin/driver.h"

#include "thorin/pass/pass.h"
#include "thorin/util/dl.h"
#include "thorin/util/sys.h"

namespace thorin {

static std::vector<fs::path> get_plugin_name_variants(std::string_view name) {
    std::vector<fs::path> names;
    names.push_back(name); // if the user gives "libthorin_foo.so"
    names.push_back(fmt("{}thorin_{}{}", dl::prefix(), name, dl::extension()));
    return names;
}

Dialect::Dialect(const std::string& plugin_path, std::unique_ptr<void, decltype(&dl::close)>&& handle)
    : plugin_path_(plugin_path)
    , handle_(std::move(handle)) {
    auto get_info =
        reinterpret_cast<decltype(&thorin_get_dialect_info)>(dl::get(this->handle(), "thorin_get_dialect_info"));

    if (!get_info) throw std::runtime_error{"dialect plugin has no thorin_get_dialect_info()"};
    info_ = get_info();
}

Dialect Dialect::load(Driver& driver, const std::string& name) {
    std::unique_ptr<void, decltype(&dl::close)> handle{nullptr, dl::close};
    std::string plugin_path = name;
    if (auto path = fs::path{name}; path.is_absolute() && fs::is_regular_file(path))
        handle.reset(dl::open(name));
    if (!handle) {
        auto name_variants = get_plugin_name_variants(name);
        for (const auto& path : driver.search_paths()) {
            for (const auto& name_variant : name_variants) {
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

    return Dialect{plugin_path, std::move(handle)};
}

} // namespace thorin
