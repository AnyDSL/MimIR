#include "thorin/util/dlopen.h"

#include <cstdlib>

#include <sstream>

#ifdef _WIN32
#    include <windows.h>
#else
#    include <dlfcn.h>
#endif

namespace thorin {
void* load_library(const std::string& filename) {
#ifdef _WIN32
    if (HMODULE handle = LoadLibraryA(filename.c_str())) {
        return static_cast<void*>(handle);
    } else {
        std::stringstream ss;
        ss << "error: could not load dialect plugin: " << filename << " with: " << GetLastError()
           << "(see https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes)" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#else
    if (void* handle = dlopen(filename.c_str(), RTLD_NOW)) {
        return handle;
    } else {
        std::stringstream ss;
        ss << "error: could not load dialect plugin: " << filename << std::endl;
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
        ss << "error: could not find symbol name in dialect plugin: " << symbol_name << " with: " << GetLastError()
           << " (https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes)" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#else
    dlerror(); // clear error state
    void* symbol = dlsym(handle, symbol_name.c_str());
    if (char* err = dlerror()) {
        std::stringstream ss;
        ss << "error: could not find symbol name in dialect plugin: " << symbol_name << std::endl;
        ss << err << std::endl;
        throw std::runtime_error{ss.str()};
    } else {
        return symbol;
    }
#endif
}

std::optional<std::filesystem::path> get_path_to_current_executable() {
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
        return std::filesystem::path{path_buffer.data()}.parent_path().parent_path() / "lib";
    }
#else
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&get_path_to_current_executable), &info)) {
        return std::filesystem::path{info.dli_fname}.parent_path().parent_path() / "lib";
    }
#endif
    // an error occurred, we don't have a path..
    return {};
}

void close_library(void* handle) {
#ifdef _WIN32
    if (!FreeLibrary(static_cast<HMODULE>(handle))) {
        std::stringstream ss;
        ss << "error: FreeLibrary() failed" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#else
    if (int err = dlclose(handle)) {
        std::stringstream ss;
        ss << "error: dlclose() failed (" << err << ")" << std::endl;
        throw std::runtime_error{ss.str()};
    }
#endif
}
} // namespace thorin
