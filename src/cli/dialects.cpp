#include "cli/dialects.h"

#include <cstdlib>

#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "thorin/config.h"
#include "thorin/world.h"

#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"

#ifdef _WIN32
#    include <windows.h>
#else
#    include <dlfcn.h>
#endif

using namespace thorin;

void* load_library(const std::string& filename) {
#ifdef _WIN32
    if (HMODULE handle = LoadLibraryA(filename.c_str())) {
        return static_cast<void*>(handle);
    } else {
        std::stringstream ss;
        ss << "Could not load dialect plugin: " << filename << " with: " << GetLastError()
           << "(see https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes)" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#else
    if (void* handle = dlopen(filename.c_str(), RTLD_NOW)) {
        return handle;
    } else {
        std::stringstream ss;
        ss << "Could not load dialect plugin: " << filename << std::endl;
        if (char* err = dlerror()) { ss << err << std::endl; }
        throw std::runtime_error{ss.str()};
    }
#endif
}

void* get_symbol_from_library(void* handle, const std::string& symbol_name) {
#ifdef _WIN32
    if (auto symbol = GetProcAddress(static_cast<HMODULE>(handle), symbol_name.c_str())) {
        return reinterpret_cast<void*>(symbol);
    } else {
        std::stringstream ss;
        ss << "Could not find symbol name in dialect plugin: " << symbol_name << " with: " << GetLastError()
           << " (https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes)" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#else
    dlerror(); // clear error state
    void* symbol = dlsym(handle, symbol_name.c_str());
    if (char* err = dlerror()) {
        std::stringstream ss;
        ss << "Could not find symbol name in dialect plugin: " << symbol_name << std::endl;
        ss << err << std::endl;
        throw std::runtime_error{ss.str()};
    } else {
        return symbol;
    }
#endif
}

void add_paths_from_env(std::vector<std::filesystem::path>& paths) {
    if (const char* env_path = std::getenv("THORIN_DIALECT_PATH")) {
        std::stringstream env_path_stream{env_path};
        std::string sub_path;
        while (std::getline(env_path_stream, sub_path, ':')) {
            std::filesystem::path path{sub_path};
            if (std::filesystem::is_directory(path)) paths.push_back(std::move(path));
        }
    }
}

std::vector<std::filesystem::path> get_plugin_search_paths(const std::vector<std::string>& user_paths) {
    std::vector<std::filesystem::path> paths{user_paths.begin(), user_paths.end()};

    add_paths_from_env(paths);

    // make user paths absolute paths.
    const auto cwd = std::filesystem::current_path();
    std::transform(paths.begin(), paths.end(), paths.begin(), [&cwd](std::filesystem::path path) {
        if (path.is_relative()) return cwd / path;
        return path;
    });

    // add path/to/thorin.exe/../../lib
#ifdef _WIN32
    std::vector<char> path_buffer;
    size_t read = 0;
    do {
        // start with 256 (almost MAX_PATH) and grow exp
        path_buffer.resize(std::max(path_buffer.size(), static_cast<size_t>(128)) * 2);
        read = GetModuleFileNameA(nullptr, path_buffer.data(), static_cast<DWORD>(path_buffer.size()));
    } while (read == path_buffer.size()); // if equal, the buffer was too small.
    if (read != 0) {
        path_buffer.resize(read);
        paths.emplace_back(std::filesystem::path{path_buffer.data()}.parent_path().parent_path() / "lib");
    }
#else
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&get_plugin_search_paths), &info)) {
        paths.emplace_back(std::filesystem::path{info.dli_fname}.parent_path().parent_path() / "lib");
    }
#endif

    // add default install path
    const auto install_prefixed_path = std::filesystem::path{THORIN_INSTALL_PREFIX} / "lib";

    if (paths.empty() ||
        (std::filesystem::is_directory(install_prefixed_path) &&
         !std::filesystem::equivalent(install_prefixed_path, paths.back()))) // avoid double checking install dir
        paths.emplace_back(std::move(install_prefixed_path));

    // add current working directory
    paths.emplace_back(std::move(cwd));
    return paths;
}

std::vector<std::filesystem::path> get_plugin_name_variants(std::string_view name) {
    std::vector<std::filesystem::path> names;
    names.push_back(name); // if the user gives "libthorin_foo.so"
    std::stringstream libName;
#ifdef _WIN32
    libName << "thorin_" << name << ".dll";
#elif defined(__APPLE__)
    libName << "libthorin_" << name << ".dylib";
#else
    libName << "libthorin_" << name << ".so";
#endif
    names.push_back(libName.str());
    return names;
}

void close_library(void* handle) {
#ifdef _WIN32
    if (!FreeLibrary(static_cast<HMODULE>(handle))) {
        std::stringstream ss;
        ss << "FreeLibrary() failed" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#else
    if (int err = dlclose(handle)) {
        std::stringstream ss;
        ss << "dlclose() failed (" << err << ")" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#endif
}

// todo: remove me again:
namespace thorin {
void test_world(World& w) {
    auto mem_t  = w.type_mem();
    auto i32_t  = w.type_int_width(32);
    auto argv_t = w.type_ptr(w.type_ptr(i32_t));

    // Cn [mem, i32, Cn [mem, i32]]
    auto main_t                 = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main                   = w.nom_lam(main_t, w.dbg("main"));
    auto [mem, argc, argv, ret] = main->vars<4>();
    main->app(false, ret, {mem, argc});
    main->make_external();
}

void test_plugin(Dialect& dialect) {
    World world;
    test_world(world);
    PipelineBuilder builder{};
    dialect.register_passes(builder);

    auto man = builder.opt_phase(world);
    for (auto& pass : man.passes()) std::cout << "pass: " << pass->name() << std::endl;
    man.run();
}
} // namespace thorin

Dialect::Dialect(const std::string& plugin_path, std::unique_ptr<void, decltype(&close_library)>&& handle)
    : plugin_path_(plugin_path)
    , handle_(std::move(handle)) {
    auto get_info = reinterpret_cast<decltype(&thorin_get_dialect_info)>(
        get_symbol_from_library(this->handle(), "thorin_get_dialect_info"));

    if (!get_info) throw std::runtime_error{"dialect plugin has no thorin_get_dialect_info()"};
    info_ = get_info();
}

Dialect Dialect::load_dialect_library(const std::string& name, const std::vector<std::string>& search_paths) {
    std::unique_ptr<void, decltype(&close_library)> handle{nullptr, close_library};
    std::string plugin_path = name;
    if (auto path = std::filesystem::path{name}; path.is_absolute() && std::filesystem::is_regular_file(path))
        handle.reset(load_library(name));
    if (!handle) {
        auto paths         = get_plugin_search_paths(search_paths);
        auto name_variants = get_plugin_name_variants(name);
        for (const auto& path : paths) {
            for (const auto& name_variant : name_variants) {
                auto full_path = path / name_variant;
                plugin_path    = full_path;

                std::error_code ignore;
                if (bool reg_file = std::filesystem::is_regular_file(full_path, ignore); reg_file && !ignore)
                    if (handle.reset(load_library(full_path.string())); handle) break;
            }
            if (handle) break;
        }
    }

    if (!handle) throw std::runtime_error("cannot open plugin");

    return Dialect{plugin_path, std::move(handle)};
}
