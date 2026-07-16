#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model.hpp"

namespace feedforge::compiler {

inline constexpr std::uint32_t ffir_format_version = 1U;

struct ffir_type_v1 {
  std::string name;
  physical_kind physical{};
  std::uint32_t width{};
  logical_kind logical{};
  std::optional<std::uint32_t> scale;
};

struct ffir_field_v1 {
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

struct ffir_message_v1 {
  std::string name;
  std::uint8_t discriminator{};
  std::uint32_t size{};
  std::vector<ffir_field_v1> fields;
};

struct ffir_event_v1 {
  std::string event;
  std::uint8_t source_discriminator{};
  std::string source_message;
  std::vector<ffir_field_v1> fields;
};

struct ffir_schema_v1 {
  std::string name;
  std::string protocol_version;
  std::string document_revision;
  std::string fingerprint;
  std::string wire_endian;
  std::uint32_t discriminator_offset{};
  std::uint32_t discriminator_width{};
  std::vector<ffir_type_v1> types;
  std::vector<ffir_message_v1> messages;
};

struct ffir_pipeline_v1 {
  std::string name;
  std::string cpp_namespace;
  std::string schema;
  std::string profile;
  std::string variant_id;
  std::string unknown_messages;
  std::string unselected_messages;
  std::string fingerprint;
  std::vector<ffir_event_v1> events;
};

struct ffir_v1 {
  std::uint32_t format_version{ffir_format_version};
  std::string generator_version;
  ffir_schema_v1 schema;
  ffir_pipeline_v1 pipeline;
};

[[nodiscard]] std::string escape_json_string(std::string_view value);
[[nodiscard]] std::string canonical_schema_semantics_json(
    const ffir_v1& ir);
[[nodiscard]] std::string canonical_pipeline_semantics_json(
    const ffir_v1& ir);
[[nodiscard]] std::string canonical_json(const ffir_v1& ir);

}  // namespace feedforge::compiler
