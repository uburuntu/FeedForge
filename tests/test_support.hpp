#ifndef FEEDFORGE_TEST_SUPPORT_HPP
#define FEEDFORGE_TEST_SUPPORT_HPP

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace feedforge::test {

[[noreturn]] inline void fail(std::string_view expression,
                              std::string_view file, int line) {
  std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
  std::abort();
}

inline void check(bool condition, std::string_view expression,
                  std::string_view file, int line) {
  if (!condition) {
    fail(expression, file, line);
  }
}

}  // namespace feedforge::test

#define FEEDFORGE_CHECK(expression)                                      \
  ::feedforge::test::check(static_cast<bool>(expression), #expression,   \
                           __FILE__, __LINE__)

#endif  // FEEDFORGE_TEST_SUPPORT_HPP
