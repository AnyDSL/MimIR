#include "thorin/dialects.h"

#include <cstdlib>

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

#include "thorin/config.h"
#include "thorin/world.h"

#include "thorin/pass/pass.h"
#include "thorin/util/dl.h"
#include "thorin/util/sys.h"

using namespace thorin;

static void add_paths_from_env(std::vector<std::filesystem::path>& paths) {
    if (const char* env_path = std::getenv("THORIN_DIALECT_PATH")) {
        std::stringstream env_path_stream{env_path};
        std::string sub_path;
        while (std::getline(env_path_stream, sub_path, ':')) {
            std::filesystem::path path{sub_path};
            if (std::filesystem::is_directory(path)) paths.push_back(std::move(path));
        }
    }
}

static std::vector<std::filesystem::path> get_plugin_name_variants(std::string_view name) {
    std::vector<std::filesystem::path> names;
    names.push_back(name); // if the user gives "libthorin_foo.so"
    names.push_back(fmt("{}thorin_{}{}", dl::prefix(), name, dl::extension()));
    return names;
}

namespace thorin {

std::vector<std::filesystem::path> get_plugin_search_paths(Span<std::string> user_paths) {
    std::vector<std::filesystem::path> paths{user_paths.begin(), user_paths.end()};

    add_paths_from_env(paths);

    // make user paths absolute paths.
    const auto cwd = std::filesystem::current_path();
    std::ranges::transform(paths, paths.begin(), [&](auto path) { return path.is_relative() ? cwd / path : path; });

    // add path/to/thorin.exe/../../lib/thorin
    if (auto path = sys::path_to_curr_exe()) paths.emplace_back(path->parent_path().parent_path() / "lib" / "thorin");

    // add default install path
    const auto install_prefixed_path = std::filesystem::path{THORIN_INSTALL_PREFIX} / "lib" / "thorin";

    if (paths.empty() ||
        (std::filesystem::is_directory(install_prefixed_path) &&
         !std::filesystem::equivalent(install_prefixed_path, paths.back()))) // avoid double checking install dir
        paths.emplace_back(std::move(install_prefixed_path));

    // add current working directory
    paths.emplace_back(std::move(cwd));
    return paths;
}

Dialect::Dialect(const std::string& plugin_path, std::unique_ptr<void, decltype(&dl::close)>&& handle)
    : plugin_path_(plugin_path)
    , handle_(std::move(handle)) {
    auto get_info =
        reinterpret_cast<decltype(&thorin_get_dialect_info)>(dl::get(this->handle(), "thorin_get_dialect_info"));

    if (!get_info) throw std::runtime_error{"dialect plugin has no thorin_get_dialect_info()"};
    info_ = get_info();
}

Dialect Dialect::load(const std::string& name, Span<std::string> search_paths) {
    std::unique_ptr<void, decltype(&dl::close)> handle{nullptr, dl::close};
    std::string plugin_path = name;
    if (auto path = std::filesystem::path{name}; path.is_absolute() && std::filesystem::is_regular_file(path))
        handle.reset(dl::open(name));
    if (!handle) {
        auto paths         = get_plugin_search_paths(search_paths);
        auto name_variants = get_plugin_name_variants(name);
        for (const auto& path : paths) {
            for (const auto& name_variant : name_variants) {
                auto full_path = path / name_variant;
                plugin_path    = full_path.string();

                std::error_code ignore;
                if (bool reg_file = std::filesystem::is_regular_file(full_path, ignore); reg_file && !ignore) {
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
