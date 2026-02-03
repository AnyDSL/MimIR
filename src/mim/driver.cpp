#include "mim/driver.h"

#include "mim/plugin.h"

#include "mim/util/dl.h"
#include "mim/util/sys.h"

namespace mim {

const fs::path* Driver::Imports::add(fs::path path, Sym sym) {
    if (!fs::exists(path)) {
        driver_.WLOG("import path '{}' does not exist", path.string());
        return nullptr;
    }
    for (auto p : paths())
        if (fs::equivalent(p, path)) return nullptr;

    path2sym_.emplace_back(std::pair(std::move(path), sym));
    return &path2sym_.back().first;
}

Driver::Driver()
    : log_(flags_)
    , world_(this)
    , imports_(*this) {
    // prepend empty path
    search_paths_.emplace_front(fs::path{});

    // paths from env
    if (auto env_path = std::getenv("MIM_PLUGIN_PATH")) {
        std::stringstream env_path_stream{env_path};
        std::string sub_path;
        while (std::getline(env_path_stream, sub_path, ':'))
            add_search_path(sub_path);
    }

    // add path/to/mim.exe/../../lib/mim
    if (auto path = sys::path_to_curr_exe()) add_search_path(path->parent_path().parent_path() / MIM_LIBDIR / "mim");

    // add install path if different from above
    if (auto install_path = fs::path{MIM_INSTALL_PREFIX} / MIM_LIBDIR / "mim"; fs::exists(install_path)) {
        if (search_paths().size() < 2 || !fs::equivalent(install_path, search_paths().back()))
            add_search_path(std::move(install_path));
    }

    // all other user paths are placed just behind the first path (the empty path)
    insert_ = ++search_paths_.begin();
}

void Driver::load(Sym name) {
    ILOG("💾 loading plugin: '{}'", name);

    if (is_loaded(name)) {
        WLOG("mim/plugin '{}' already loaded", name);
        return;
    }

    auto handle = Plugin::Handle{nullptr, dl::close};
    if (auto path = fs::path{name.view()}; path.is_absolute() && fs::is_regular_file(path))
        handle.reset(dl::open(name.c_str()));
    if (!handle) {
        for (const auto& path : search_paths()) {
            auto full_path = path / fmt("libmim_{}.{}", name, dl::extension);
            std::error_code ignore;
            if (bool reg_file = fs::is_regular_file(full_path, ignore); reg_file && !ignore) {
                auto path_str = full_path.string();
                if (handle.reset(dl::open(path_str.c_str())); handle) break;
            }
            if (handle) break;
        }
    }

    if (!handle) error("cannot open plugin '{}'", name);

    if (auto get_info = reinterpret_cast<decltype(&mim_get_plugin)>(dl::get(handle.get(), "mim_get_plugin"))) {
        assert_emplace(plugins_, name, std::move(handle));
        auto info = get_info();
        // clang-format off
        if (auto reg = info.register_normalizers) reg(normalizers_);
        if (auto reg = info.register_stages)      reg(stages_);
        if (auto reg = info.register_backends)    reg(backends_);
        // clang-format on
    } else {
        error("mim/plugin has no 'mim_get_plugin()'");
    }
}

void* Driver::get_fun_ptr(Sym plugin, const char* name) {
    if (auto handle = lookup(plugins_, plugin)) return dl::get(handle->get(), name);
    return nullptr;
}

} // namespace mim
