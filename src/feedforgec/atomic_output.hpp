#pragma once

#include <string_view>

#include "diagnostics.hpp"

namespace feedforge::compiler {

[[nodiscard]] result<void> write_file_atomically(std::string_view destination,
                                                 std::string_view contents,
                                                 std::string_view object_path);

} // namespace feedforge::compiler
