#pragma once

#include <stdexcept>
#include <string>

namespace thorin::dl {

static constexpr auto extensions = {
#ifdef _WIN32
    "dll" // Windows
#elif defined(__APPLE__) && defined(__MACH_)
    "so", "dylib", // MacOs
#else
    "so" // Linux, etc
#endif
};

void* open(const char* filename);
void* get(void* handle, const char* symbol_name);
void close(void* handle);

} // namespace thorin::dl
