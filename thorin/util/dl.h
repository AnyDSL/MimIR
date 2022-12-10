#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace thorin::dl {

class Error : public std::runtime_error {
public:
    Error(const std::string& what_arg)
        : std::runtime_error(what_arg) {}
};

std::string_view prefix();    ///< `"lib"` or `""`
std::string_view extension(); ///< `".dll"` or `".so"`

void* open(const std::string& filename);
void* get(void* handle, const std::string& symbol_name);
void close(void* handle);

} // namespace thorin::dl
