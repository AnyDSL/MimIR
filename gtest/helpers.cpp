#include <algorithm>

#include <gtest/gtest.h>

namespace thorin::gtest {

std::string test_name() {
    std::string s = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::ranges::transform(s.begin(), s.end(), s.begin(), [](auto c) { return c == '/' ? '_' : c; });
    return s;
}

} // namespace thorin::gtest
