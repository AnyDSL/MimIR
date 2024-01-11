#pragma once

#include <stdexcept>
#include <string>

namespace thorin::dl {

static constexpr auto extension =
#if defined(_WIN32)
    "dll";
#else
    "so";
#endif

void* open(const char* filename);
void* get(void* handle, const char* symbol_name);
void close(void* handle);

} // namespace thorin::dl
