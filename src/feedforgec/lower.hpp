#pragma once

#include <string_view>

#include "ir.hpp"
#include "model.hpp"

namespace feedforge::compiler {

[[nodiscard]] ffir_v1 lower_to_ffir(const resolved_schema& schema,
                                    const resolved_pipeline& pipeline,
                                    std::string_view generator_version);

}  // namespace feedforge::compiler
