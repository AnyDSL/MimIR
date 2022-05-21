#ifndef THORIN_CLI_DIALECTS_H
#define THORIN_CLI_DIALECTS_H

#include <string>
#include <vector>

namespace thorin::cli {

void test_plugin(const std::string& name, const std::vector<std::string>& search_paths);

} // namespace thorin::cli

#endif
