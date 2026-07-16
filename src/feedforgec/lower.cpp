#include "lower.hpp"

#include <algorithm>
#include <string>

#include "sha256.hpp"

namespace feedforge::compiler {
namespace {

[[nodiscard]] ffir_field_v1 lower_field(const resolved_field& field) {
  return {
      .name = field.name,
      .type_name = field.type_name,
      .physical = field.physical,
      .logical = field.logical,
      .offset = field.offset,
      .width = field.width,
      .scale = field.scale,
      .discriminator = field.discriminator,
      .projectable = field.projectable,
  };
}

}  // namespace

ffir_v1 lower_to_ffir(const resolved_schema& schema,
                      const resolved_pipeline& pipeline,
                      const std::string_view generator_version) {
  ffir_v1 ir{
      .format_version = ffir_format_version,
      .generator_version = std::string(generator_version),
      .schema =
          {
              .name = schema.name,
              .protocol_version = schema.protocol_version,
              .document_revision = schema.document_revision,
              .fingerprint = {},
              .wire_endian = schema.wire_endian,
              .discriminator_offset = schema.discriminator_offset,
              .discriminator_width = schema.discriminator_width,
              .types = {},
              .messages = {},
          },
      .pipeline =
          {
              .name = pipeline.name,
              .cpp_namespace = pipeline.cpp_namespace,
              .schema = pipeline.schema,
              .profile = pipeline.profile,
              .variant_id = pipeline.variant_id,
              .unknown_messages = pipeline.unknown_messages,
              .unselected_messages = pipeline.unselected_messages,
              .fingerprint = {},
              .events = {},
          },
  };

  ir.schema.types.reserve(schema.types.size());
  for (const resolved_type& type : schema.types) {
    ir.schema.types.push_back(ffir_type_v1{
        .name = type.name,
        .physical = type.physical,
        .width = type.width,
        .logical = type.logical,
        .scale = type.scale,
    });
  }
  std::ranges::sort(ir.schema.types, {}, &ffir_type_v1::name);

  ir.schema.messages.reserve(schema.messages.size());
  for (const resolved_message& message : schema.messages) {
    ffir_message_v1 lowered_message{
        .name = message.name,
        .discriminator = message.discriminator,
        .size = message.size,
        .fields = {},
    };
    lowered_message.fields.reserve(message.fields.size());
    for (const resolved_field& field : message.fields) {
      lowered_message.fields.push_back(lower_field(field));
    }
    std::ranges::sort(
        lowered_message.fields, {},
        [](const ffir_field_v1& field) {
          return std::pair{field.offset, field.name};
        });
    ir.schema.messages.push_back(std::move(lowered_message));
  }
  std::ranges::sort(ir.schema.messages, {},
                    &ffir_message_v1::discriminator);

  ir.pipeline.events.reserve(pipeline.projections.size());
  for (const resolved_projection& projection : pipeline.projections) {
    ffir_event_v1 event{
        .event = projection.event,
        .source_discriminator = projection.source_discriminator,
        .source_message = projection.source_message,
        .fields = {},
    };
    event.fields.reserve(projection.fields.size());
    for (const resolved_field& field : projection.fields) {
      event.fields.push_back(lower_field(field));
    }
    ir.pipeline.events.push_back(std::move(event));
  }
  std::ranges::sort(ir.pipeline.events, {},
                    &ffir_event_v1::source_discriminator);

  ir.schema.fingerprint =
      sha256_hex(canonical_schema_semantics_json(ir));
  ir.pipeline.fingerprint =
      sha256_hex(canonical_pipeline_semantics_json(ir));
  return ir;
}

}  // namespace feedforge::compiler
