#pragma once

#include <cstddef>

namespace feedforge::compiler::limits {

inline constexpr std::size_t source_bytes = 1U << 20U;
inline constexpr std::size_t toml_nested_values = 32U;
inline constexpr std::size_t toml_nodes = 32768U;
inline constexpr std::size_t identifier_bytes = 128U;
inline constexpr std::size_t namespace_bytes = 512U;
inline constexpr std::size_t namespace_components = 16U;
inline constexpr std::size_t documentation_bytes = 4096U;
inline constexpr std::size_t user_types = 256U;
inline constexpr std::size_t messages = 94U;
inline constexpr std::size_t projections = 94U;
inline constexpr std::size_t fields_per_message = 1024U;
inline constexpr std::size_t fields_per_projection = 1024U;
inline constexpr std::size_t total_schema_fields = 8192U;
inline constexpr std::size_t total_projected_fields = 8192U;
inline constexpr std::size_t allowed_values_per_field = 256U;
inline constexpr std::size_t allowed_value_bytes_per_field = 65535U;
inline constexpr std::size_t rendered_bytes = 16U << 20U;

} // namespace feedforge::compiler::limits
