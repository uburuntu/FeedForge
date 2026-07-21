#ifndef FEEDFORGE_COMPILER_FUZZ_SUPPORT_HPP
#define FEEDFORGE_COMPILER_FUZZ_SUPPORT_HPP

#include "fuzz_support.hpp"

#include "diagnostics.hpp"
#include "model.hpp"
#include "parse_toml.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace feedforge::fuzz {

inline constexpr std::string_view valid_schema_toml = R"toml(format_version = 1
name = "test_feed"
protocol_version = "1.0"
document_revision = "fuzz"
wire_endian = "big"
discriminator_offset = 0
discriminator_width = 1

[[messages]]
name = "add_order"
type = "A"
size = 8

[[messages.fields]]
name = "message_type"
type = "alpha"
offset = 0
width = 1
role = "discriminator"
value = "A"

[[messages.fields]]
name = "order_id"
type = "u16"
offset = 1
width = 2

[[messages.fields]]
name = "price"
type = "u32"
offset = 3
width = 4

[[messages.fields]]
name = "side"
type = "alpha"
offset = 7
width = 1

[[messages]]
name = "order_update"
type = "U"
size = 4

[[messages.fields]]
name = "message_type"
type = "alpha"
offset = 0
width = 1
role = "discriminator"
value = "U"

[[messages.fields]]
name = "quantity"
type = "u16"
offset = 1
width = 2

[[messages.fields]]
name = "code"
type = "alpha"
offset = 3
width = 1
)toml";

inline constexpr std::string_view valid_pipeline_toml = R"toml(format_version = 1
name = "test_projection"
namespace = "feedforge::generated::test_projection"
schema = "test_feed"
profile = "portable_checked"
unknown_messages = "error"
unselected_messages = "skip"

[[emit]]
source = "U"
event = "order_update"
fields = ["*"]

[[emit]]
source = "A"
event = "add_order"
fields = ["price", "side"]
)toml";

inline constexpr std::string_view compiler_case_separator = "\n# --- FEEDFORGE FUZZ PIPELINE ---\n";

[[nodiscard]] inline std::string_view as_text(const std::uint8_t* const data,
                                              const std::size_t size) noexcept {
  return {reinterpret_cast<const char*>(data), size};
}

inline void require_same_diagnostic(const compiler::diagnostic& left,
                                    const compiler::diagnostic& right) noexcept {
  require(left.code == right.code);
  require(left.path == right.path);
  require(left.position == right.position);
  require(left.object_path == right.object_path);
  require(left.message == right.message);
  require(left.hint == right.hint);
}

[[nodiscard]] inline const compiler::resolved_schema& reference_schema() {
  static const compiler::resolved_schema schema = [] {
    auto parsed = compiler::parse_schema_toml(valid_schema_toml, "fuzz/reference-schema.toml");
    require(parsed.has_value());
    auto validated = compiler::validate_schema(*parsed);
    require(validated.has_value());
    return std::move(*validated);
  }();
  return schema;
}

[[nodiscard]] inline const std::string& valid_compiler_case() {
  static const std::string input = [] {
    std::string value;
    value.reserve(valid_schema_toml.size() + compiler_case_separator.size() +
                  valid_pipeline_toml.size());
    value.append(valid_schema_toml);
    value.append(compiler_case_separator);
    value.append(valid_pipeline_toml);
    return value;
  }();
  return input;
}

} // namespace feedforge::fuzz

#endif // FEEDFORGE_COMPILER_FUZZ_SUPPORT_HPP
