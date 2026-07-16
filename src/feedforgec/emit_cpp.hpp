#pragma once

#include <string>

#include "diagnostics.hpp"
#include "ir.hpp"

namespace feedforge::compiler {

[[nodiscard]] result<std::string> emit_cpp(const ffir_v1& ir);

}  // namespace feedforge::compiler
