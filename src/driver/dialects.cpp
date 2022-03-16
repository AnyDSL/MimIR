#include "driver/dialects.h"

#include <filesystem>
#include <sstream>
#include <string>

#include "thorin/config.h"
#include "thorin/world.h"

#include "thorin/pass/pass.h"

#ifdef _WIN32
#    include <windows.h>
#    define dlsym  GetProcAddress
#    define popen  _popen
#    define pclose _pclose
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

void* get_symbol_from_library(void* handle, const std::string& symbolName) {
#ifdef _WIN32
    if (FARPROC symbol = GetProcAddress(static_cast<HMODULE>(handle), symbolName.c_str())) {
        return reinterpret_cast<void*>(symbol);
    } else {
        std::stringstream ss;
        ss << "Could not find symbol name in dialect plugin: " << symbolName << " with: " << GetLastError()
           << " (https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes)" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#else
    dlerror(); // clear error state
    void* symbol = dlsym(handle, symbolName.c_str());
    if (char* err = dlerror()) {
        std::stringstream ss;
        ss << "Could not find symbol name in dialect plugin: " << symbolName << std::endl;
        ss << err << std::endl;
        throw std::runtime_error{ss.str()};
    } else {
        return symbol;
    }
#endif
}

std::vector<std::filesystem::path> get_plugin_search_paths() {
    std::vector<std::filesystem::path> paths;
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
    const auto install_prefixed_path = std::filesystem::path{THORIN_INSTALL_PREFIX} / "lib";

    if (paths.empty() ||
        (std::filesystem::is_directory(install_prefixed_path) &&
         !std::filesystem::equivalent(install_prefixed_path, paths.back()))) // avoid double checking install dir
        paths.emplace_back(std::move(install_prefixed_path));

    std::error_code ec;
    if (auto cwd = std::filesystem::current_path(ec); !ec) paths.emplace_back(std::move(cwd));

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

void test_plugin(const std::string& name) {
    void* handle = nullptr;
    if (auto path = std::filesystem::path{name}; path.is_absolute() && std::filesystem::is_regular_file(path))
        handle = load_library(name);
    if (!handle) {
        auto paths         = get_plugin_search_paths();
        auto name_variants = get_plugin_name_variants(name);
        for (const auto& path : paths) {
            for (const auto& name_variant : name_variants) {
                auto full_path = path / name_variant;
                std::error_code ignore;
                if (bool reg_file = std::filesystem::is_regular_file(full_path, ignore); reg_file && !ignore)
                    if ((handle = load_library(full_path.string()))) break;
            }
            if (handle) break;
        }
    }

    if (!handle) throw std::runtime_error("cannot open plugin");

    auto create  = (CreateIPass)get_symbol_from_library(handle, "create");
    auto destroy = (DestroyIPass)get_symbol_from_library(handle, "destroy");

    if (!create || !destroy) throw std::runtime_error("cannot find symbol");

    World world;
    PassMan man(world);
    auto pass = create(man);
    outln("hi from: '{}'", pass->name());
    destroy(pass);

    close_library(handle);
}
