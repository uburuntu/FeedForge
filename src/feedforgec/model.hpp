#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "diagnostics.hpp"

namespace feedforge::compiler {

struct type_source {
  source_mark mark;
  std::string name;
  std::string kind;
  std::int64_t width{};
  std::optional<std::string> logical;
  std::optional<std::int64_t> scale;
};

struct field_source {
  source_mark mark;
  std::string name;
  std::string type;
  std::int64_t offset{};
  std::int64_t width{};
  std::optional<std::string> role;
  std::optional<std::string> value;
  std::vector<std::string> allowed;
};

struct message_source {
  source_mark mark;
  std::string name;
  std::string type;
  std::int64_t size{};
  std::vector<field_source> fields;
};

struct schema_source {
  source_mark mark;
  std::string source_path;
  std::int64_t format_version{};
  std::string name;
  std::string protocol_version;
  std::string document_revision;
  std::string wire_endian;
  std::int64_t discriminator_offset{};
  std::int64_t discriminator_width{};
  std::vector<type_source> types;
  std::vector<message_source> messages;
};

struct projection_source {
  source_mark mark;
  std::string source;
  std::string event;
  std::vector<std::string> fields;
  std::vector<source_mark> field_marks;
};

struct pipeline_source {
  source_mark mark;
  std::string source_path;
  std::int64_t format_version{};
  std::string name;
  std::string cpp_namespace;
  std::string schema;
  std::string profile;
  std::string unknown_messages;
  std::string unselected_messages;
  std::vector<projection_source> projections;
};

enum class physical_kind {
  unsigned_integer,
  ascii,
  reserved,
};

enum class logical_kind {
  raw_unsigned,
  timestamp_ns,
  decimal,
  stock_locate,
  tracking_number,
  order_reference_number,
  match_number,
  share_count,
  ascii,
};

struct resolved_type {
  std::string name;
  physical_kind physical{};
  std::uint32_t width{};
  logical_kind logical{};
  std::optional<std::uint32_t> scale;
};

struct resolved_field {
  std::string name;
  std::string type_name;
  physical_kind physical{};
  std::optional<logical_kind> logical;
  std::uint32_t offset{};
  std::uint32_t width{};
  std::optional<std::uint32_t> scale;
  bool discriminator{};
  bool projectable{};
};

struct resolved_message {
  std::string name;
  std::uint8_t discriminator{};
  std::uint32_t size{};
  std::vector<resolved_field> fields;
};

struct resolved_schema {
  std::uint32_t format_version{1};
  std::string name;
  std::string protocol_version;
  std::string document_revision;
  std::string wire_endian;
  std::uint32_t discriminator_offset{};
  std::uint32_t discriminator_width{};
  std::vector<resolved_type> types;
  std::vector<resolved_message> messages;
};

struct resolved_projection {
  std::uint8_t source_discriminator{};
  std::string source_message;
  std::string event;
  std::vector<resolved_field> fields;
};

struct resolved_pipeline {
  std::uint32_t format_version{1};
  std::string name;
  std::string cpp_namespace;
  std::string schema;
  std::string profile;
  std::string variant_id;
  std::string unknown_messages;
  std::string unselected_messages;
  std::vector<resolved_projection> projections;
};

[[nodiscard]] std::string_view to_string(physical_kind kind) noexcept;
[[nodiscard]] std::string_view to_string(logical_kind kind) noexcept;

[[nodiscard]] bool is_valid_source_name(std::string_view name) noexcept;
[[nodiscard]] bool is_valid_namespace_component(
    std::string_view name) noexcept;
[[nodiscard]] bool is_cpp_keyword(std::string_view name) noexcept;

[[nodiscard]] result<resolved_schema> validate_schema(
    const schema_source& source);
[[nodiscard]] result<resolved_pipeline> validate_pipeline(
    const pipeline_source& source, const resolved_schema& schema);

}  // namespace feedforge::compiler
