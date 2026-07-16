#pragma once

#include <string_view>

#include "diagnostics.hpp"
#include "model.hpp"

namespace feedforge::compiler {

[[nodiscard]] result<schema_source> parse_schema_toml(
    std::string_view text, std::string_view source_path);
[[nodiscard]] result<pipeline_source> parse_pipeline_toml(
    std::string_view text, std::string_view source_path);

[[nodiscard]] result<schema_source> parse_schema_file(
    std::string_view source_path);
[[nodiscard]] result<pipeline_source> parse_pipeline_file(
    std::string_view source_path);

}  // namespace feedforge::compiler
