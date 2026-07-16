#include "ir.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace feedforge::compiler {
namespace {

void append_quoted(std::string& output, const std::string_view value) {
  output.push_back('"');
  output += escape_json_string(value);
  output.push_back('"');
}

void append_unsigned(std::string& output, const std::uint64_t value) {
  output += std::to_string(value);
}

void append_bool(std::string& output, const bool value) {
  output += value ? "true" : "false";
}

void append_type(std::string& output, const ffir_type_v1& type) {
  output += "{\"kind\":";
  append_quoted(output, to_string(type.physical));
  output += ",\"logical\":";
  append_quoted(output, to_string(type.logical));
  output += ",\"name\":";
  append_quoted(output, type.name);
  if (type.scale) {
    output += ",\"scale\":";
    append_unsigned(output, *type.scale);
  }
  output += ",\"width\":";
  append_unsigned(output, type.width);
  output.push_back('}');
}

void append_field(std::string& output, const ffir_field_v1& field) {
  output += "{\"discriminator\":";
  append_bool(output, field.discriminator);
  output += ",\"kind\":";
  append_quoted(output, to_string(field.physical));
  if (field.logical) {
    output += ",\"logical\":";
    append_quoted(output, to_string(*field.logical));
  }
  output += ",\"name\":";
  append_quoted(output, field.name);
  output += ",\"offset\":";
  append_unsigned(output, field.offset);
  output += ",\"projectable\":";
  append_bool(output, field.projectable);
  if (field.scale) {
    output += ",\"scale\":";
    append_unsigned(output, *field.scale);
  }
  output += ",\"type\":";
  append_quoted(output, field.type_name);
  output += ",\"width\":";
  append_unsigned(output, field.width);
  output.push_back('}');
}

void append_types(std::string& output, const std::vector<ffir_type_v1>& types) {
  output.push_back('[');
  bool first = true;
  for (const ffir_type_v1& type : types) {
    if (!first) {
      output.push_back(',');
    }
    first = false;
    append_type(output, type);
  }
  output.push_back(']');
}

void append_fields(std::string& output,
                   const std::vector<ffir_field_v1>& fields) {
  output.push_back('[');
  bool first = true;
  for (const ffir_field_v1& field : fields) {
    if (!first) {
      output.push_back(',');
    }
    first = false;
    append_field(output, field);
  }
  output.push_back(']');
}

void append_messages(std::string& output,
                     const std::vector<ffir_message_v1>& messages) {
  output.push_back('[');
  bool first = true;
  for (const ffir_message_v1& message : messages) {
    if (!first) {
      output.push_back(',');
    }
    first = false;
    output += "{\"discriminator\":";
    append_unsigned(output, message.discriminator);
    output += ",\"fields\":";
    append_fields(output, message.fields);
    output += ",\"name\":";
    append_quoted(output, message.name);
    output += ",\"size\":";
    append_unsigned(output, message.size);
    output.push_back('}');
  }
  output.push_back(']');
}

void append_events(std::string& output,
                   const std::vector<ffir_event_v1>& events) {
  output.push_back('[');
  bool first = true;
  for (const ffir_event_v1& event : events) {
    if (!first) {
      output.push_back(',');
    }
    first = false;
    output += "{\"event\":";
    append_quoted(output, event.event);
    output += ",\"fields\":";
    append_fields(output, event.fields);
    output += ",\"source\":";
    const char source_character =
        static_cast<char>(event.source_discriminator);
    append_quoted(output, std::string_view{&source_character, 1U});
    output += ",\"source_discriminator\":";
    append_unsigned(output, event.source_discriminator);
    output += ",\"source_message\":";
    append_quoted(output, event.source_message);
    output.push_back('}');
  }
  output.push_back(']');
}

void append_wire(std::string& output, const ffir_schema_v1& schema) {
  output += "{\"discriminator_offset\":";
  append_unsigned(output, schema.discriminator_offset);
  output += ",\"discriminator_width\":";
  append_unsigned(output, schema.discriminator_width);
  output += ",\"endian\":";
  append_quoted(output, schema.wire_endian);
  output.push_back('}');
}

void append_schema_semantics(std::string& output, const ffir_v1& ir) {
  output += "{\"format_version\":";
  append_unsigned(output, ir.format_version);
  output += ",\"messages\":";
  append_messages(output, ir.schema.messages);
  output += ",\"name\":";
  append_quoted(output, ir.schema.name);
  output += ",\"protocol_version\":";
  append_quoted(output, ir.schema.protocol_version);
  output += ",\"types\":";
  append_types(output, ir.schema.types);
  output += ",\"wire\":";
  append_wire(output, ir.schema);
  output.push_back('}');
}

void append_pipeline_semantics(std::string& output, const ffir_v1& ir) {
  output += "{\"events\":";
  append_events(output, ir.pipeline.events);
  output += ",\"format_version\":";
  append_unsigned(output, ir.format_version);
  output += ",\"name\":";
  append_quoted(output, ir.pipeline.name);
  output += ",\"namespace\":";
  append_quoted(output, ir.pipeline.cpp_namespace);
  output += ",\"profile\":";
  append_quoted(output, ir.pipeline.profile);
  output += ",\"schema\":";
  append_quoted(output, ir.pipeline.schema);
  output += ",\"unknown_messages\":";
  append_quoted(output, ir.pipeline.unknown_messages);
  output += ",\"unselected_messages\":";
  append_quoted(output, ir.pipeline.unselected_messages);
  output += ",\"variant_id\":";
  append_quoted(output, ir.pipeline.variant_id);
  output.push_back('}');
}

}  // namespace

std::string escape_json_string(const std::string_view value) {
  constexpr std::string_view hexadecimal{"0123456789abcdef"};
  std::string escaped;
  escaped.reserve(value.size());
  for (const char raw_character : value) {
    const auto character = static_cast<unsigned char>(raw_character);
    switch (character) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (character < 0x20U) {
          escaped += "\\u00";
          escaped.push_back(
              hexadecimal[static_cast<std::size_t>(character >> 4U)]);
          escaped.push_back(
              hexadecimal[static_cast<std::size_t>(character & 0x0fU)]);
        } else {
          escaped.push_back(static_cast<char>(character));
        }
        break;
    }
  }
  return escaped;
}

std::string canonical_schema_semantics_json(const ffir_v1& ir) {
  std::string output;
  output.reserve(1024U);
  append_schema_semantics(output, ir);
  return output;
}

std::string canonical_pipeline_semantics_json(const ffir_v1& ir) {
  std::string output;
  output.reserve(512U);
  append_pipeline_semantics(output, ir);
  return output;
}

std::string canonical_json(const ffir_v1& ir) {
  std::string output;
  output.reserve(2048U);
  output += "{\"format_version\":";
  append_unsigned(output, ir.format_version);
  output += ",\"generator_version\":";
  append_quoted(output, ir.generator_version);
  output += ",\"pipeline\":{\"events\":";
  append_events(output, ir.pipeline.events);
  output += ",\"fingerprint\":";
  append_quoted(output, ir.pipeline.fingerprint);
  output += ",\"name\":";
  append_quoted(output, ir.pipeline.name);
  output += ",\"namespace\":";
  append_quoted(output, ir.pipeline.cpp_namespace);
  output += ",\"profile\":";
  append_quoted(output, ir.pipeline.profile);
  output += ",\"schema\":";
  append_quoted(output, ir.pipeline.schema);
  output += ",\"unknown_messages\":";
  append_quoted(output, ir.pipeline.unknown_messages);
  output += ",\"unselected_messages\":";
  append_quoted(output, ir.pipeline.unselected_messages);
  output += ",\"variant_id\":";
  append_quoted(output, ir.pipeline.variant_id);
  output += "},\"schema\":{\"document_revision\":";
  append_quoted(output, ir.schema.document_revision);
  output += ",\"fingerprint\":";
  append_quoted(output, ir.schema.fingerprint);
  output += ",\"messages\":";
  append_messages(output, ir.schema.messages);
  output += ",\"name\":";
  append_quoted(output, ir.schema.name);
  output += ",\"protocol_version\":";
  append_quoted(output, ir.schema.protocol_version);
  output += ",\"types\":";
  append_types(output, ir.schema.types);
  output += ",\"wire\":";
  append_wire(output, ir.schema);
  output += "}}\n";
  return output;
}

}  // namespace feedforge::compiler
