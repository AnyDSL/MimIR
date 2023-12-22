#pragma once

#include <stdexcept>
#include <string>

namespace thorin::dl {

static constexpr auto extensions = {
#if defined(_WIN32)
    "dll" // Windows
#elif defined(__APPLE__)
    "so", "dylib", // MacOs
#else
    "so" // Linux, etc
#endif
};

void* open(const char* filename);
void* get(void* handle, const char* symbol_name);
void close(void* handle);

} // namespace thorin::dl
