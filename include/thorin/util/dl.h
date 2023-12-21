#pragma once

#include <stdexcept>
#include <string>

namespace thorin::dl {

static constexpr auto extensions = {"dll", "dylib", "so"};

void* open(const char* filename);
void* get(void* handle, const char* symbol_name);
void close(void* handle);

} // namespace thorin::dl
