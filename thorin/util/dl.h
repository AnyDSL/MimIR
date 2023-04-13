#pragma once

#include <stdexcept>
#include <string>

namespace thorin::dl {

std::string_view extension(); ///< `".dll"` or `".so"`

void* open(const std::string& filename);
void* get(void* handle, const std::string& symbol_name);
void close(void* handle);

} // namespace thorin::dl
