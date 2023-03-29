#include "thorin/util/dl.h"

#include <cstdlib>

#include <sstream>

#include "thorin/util/print.h"

#ifdef _WIN32
#    include <windows.h>
#else
#    include <dlfcn.h>
#endif

namespace thorin::dl {

std::string_view extension() {
#ifdef _WIN32
    return ".dll";
#else
    return ".so";
#endif
}

void* open(const std::string& file) {
#ifdef _WIN32
    if (HMODULE handle = LoadLibraryA(file.c_str())) {
        return static_cast<void*>(handle);
    } else {
        err("could not load plugin '{}' due to error '{}'\n"
            "see https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes\n",
            file, GetLastError());
    }
#else
    if (void* handle = dlopen(file.c_str(), RTLD_NOW)) {
        return handle;
    } else {
        if (char* error = dlerror())
            err("could not load plugin '{}' due to error '{}'\n", file, error);
        else
            err("could not load plugin '{}'\n", file);
    }
#endif
}

void* get(void* handle, const std::string& symbol) {
#ifdef _WIN32
    if (auto addr = GetProcAddress(static_cast<HMODULE>(handle), symbol.c_str())) {
        return reinterpret_cast<void*>(addr);
    } else {
        err("could not find symbol '{}' in plugin due to error '{}'\n"
            "see (https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes)\n",
            symbol, GetLastError());
    }
#else
    dlerror(); // clear error state
    void* addr = dlsym(handle, symbol.c_str());
    if (char* error = dlerror()) {
        err("could not find symbol '{}' in plugin due to error '{}' \n", symbol, error);
    } else {
        return addr;
    }
#endif
}

void close(void* handle) {
#ifdef _WIN32
    if (!FreeLibrary(static_cast<HMODULE>(handle))) err("FreeLibrary() failed\n");
#else
    if (int error = dlclose(handle)) err("error: dlclose() failed with error code '{}'\n", error);
#endif
}

} // namespace thorin::dl
