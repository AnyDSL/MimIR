#pragma once

#include <stdexcept>
#include <string>

namespace thorin::dl {

std::string_view extension(); ///< `".dll"` or `".so"`

void* open(const char* filename);
void* get(void* handle, const char* symbol_name);
void close(void* handle);

} // namespace thorin::dl
