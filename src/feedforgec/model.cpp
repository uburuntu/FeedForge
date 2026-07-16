#include "model.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace feedforge::compiler {
namespace {

constexpr std::array<std::string_view, 92> cpp_keywords{
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char16_t",
    "char32_t",
    "char8_t",
    "class",
    "co_await",
    "co_return",
    "co_yield",
    "compl",
    "concept",
    "const",
    "const_cast",
    "consteval",
    "constexpr",
    "constinit",
    "continue",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq",
};

constexpr std::array<std::string_view, 7> builtin_names{
    "alpha", "reserved", "u16", "u32", "u48", "u64", "u8",
};

constexpr std::array<std::string_view, 4> generated_pipeline_declarations{
    "basic_decoder",
    "decoder",
    "pipeline_metadata",
    "sink_for_all_selected_events",
};

struct type_description {
  physical_kind physical{};
  std::uint32_t width{};
  std::optional<logical_kind> logical;
  std::optional<std::uint32_t> scale;
  bool variable_width{};
};

[[nodiscard]] bool is_ascii_lower(const char character) noexcept {
  return character >= 'a' && character <= 'z';
}

[[nodiscard]] bool is_ascii_upper(const char character) noexcept {
  return character >= 'A' && character <= 'Z';
}

[[nodiscard]] bool is_ascii_digit(const char character) noexcept {
  return character >= '0' && character <= '9';
}

[[nodiscard]] diagnostic schema_problem(
    const schema_source& source, const source_mark& mark, std::string code,
    std::string object_path, std::string message,
    std::optional<std::string> hint = std::nullopt) {
  return make_diagnostic(std::move(code), source.source_path, mark,
                         std::move(object_path), std::move(message),
                         std::move(hint));
}

[[nodiscard]] diagnostic pipeline_problem(
    const pipeline_source& source, const source_mark& mark, std::string code,
    std::string object_path, std::string message,
    std::optional<std::string> hint = std::nullopt) {
  return make_diagnostic(std::move(code), source.source_path, mark,
                         std::move(object_path), std::move(message),
                         std::move(hint));
}

[[nodiscard]] std::optional<logical_kind> parse_logical(
    const std::string_view name) noexcept {
  if (name == "raw_unsigned") {
    return logical_kind::raw_unsigned;
  }
  if (name == "timestamp_ns") {
    return logical_kind::timestamp_ns;
  }
  if (name == "decimal") {
    return logical_kind::decimal;
  }
  if (name == "stock_locate") {
    return logical_kind::stock_locate;
  }
  if (name == "tracking_number") {
    return logical_kind::tracking_number;
  }
  if (name == "order_reference_number") {
    return logical_kind::order_reference_number;
  }
  if (name == "match_number") {
    return logical_kind::match_number;
  }
  if (name == "share_count") {
    return logical_kind::share_count;
  }
  if (name == "ascii") {
    return logical_kind::ascii;
  }
  return std::nullopt;
}

[[nodiscard]] bool compatible(const physical_kind physical,
                              const std::uint32_t width,
                              const logical_kind logical) noexcept {
  switch (logical) {
    case logical_kind::raw_unsigned:
      return physical == physical_kind::unsigned_integer &&
             (width == 1U || width == 2U || width == 4U || width == 6U ||
              width == 8U);
    case logical_kind::ascii:
      return physical == physical_kind::ascii && width > 0U;
    case logical_kind::timestamp_ns:
      return physical == physical_kind::unsigned_integer && width == 6U;
    case logical_kind::decimal:
      return physical == physical_kind::unsigned_integer &&
             (width == 4U || width == 8U);
    case logical_kind::stock_locate:
    case logical_kind::tracking_number:
      return physical == physical_kind::unsigned_integer && width == 2U;
    case logical_kind::order_reference_number:
    case logical_kind::match_number:
      return physical == physical_kind::unsigned_integer && width == 8U;
    case logical_kind::share_count:
      return physical == physical_kind::unsigned_integer && width == 4U;
  }
  return false;
}

[[nodiscard]] std::optional<type_description> builtin_type(
    const std::string_view name, const std::uint32_t field_width) noexcept {
  if (name == "u8") {
    return type_description{physical_kind::unsigned_integer, 1U,
                            logical_kind::raw_unsigned, std::nullopt, false};
  }
  if (name == "u16") {
    return type_description{physical_kind::unsigned_integer, 2U,
                            logical_kind::raw_unsigned, std::nullopt, false};
  }
  if (name == "u32") {
    return type_description{physical_kind::unsigned_integer, 4U,
                            logical_kind::raw_unsigned, std::nullopt, false};
  }
  if (name == "u48") {
    return type_description{physical_kind::unsigned_integer, 6U,
                            logical_kind::raw_unsigned, std::nullopt, false};
  }
  if (name == "u64") {
    return type_description{physical_kind::unsigned_integer, 8U,
                            logical_kind::raw_unsigned, std::nullopt, false};
  }
  if (name == "alpha") {
    return type_description{physical_kind::ascii, field_width,
                            logical_kind::ascii, std::nullopt, true};
  }
  if (name == "reserved") {
    return type_description{physical_kind::reserved, field_width, std::nullopt,
                            std::nullopt, true};
  }
  return std::nullopt;
}

[[nodiscard]] std::string type_object(const std::size_t index,
                                      const std::string& name) {
  if (!name.empty()) {
    return "schema.types." + name;
  }
  return "schema.types[" + std::to_string(index) + ']';
}

[[nodiscard]] std::string message_object(const std::size_t index,
                                         const std::string& name) {
  if (!name.empty()) {
    return "schema.messages." + name;
  }
  return "schema.messages[" + std::to_string(index) + ']';
}

[[nodiscard]] std::string field_object(const std::string& message_path,
                                       const std::size_t index,
                                       const std::string& name) {
  if (!name.empty()) {
    return message_path + ".fields." + name;
  }
  return message_path + ".fields[" + std::to_string(index) + ']';
}

[[nodiscard]] std::string projection_object(const std::size_t index,
                                            const std::string& event) {
  if (!event.empty()) {
    return "pipeline.emit." + event;
  }
  return "pipeline.emit[" + std::to_string(index) + ']';
}

[[nodiscard]] bool has_cpp_representation(
    const resolved_field& field) noexcept {
  return field.projectable && field.logical &&
         compatible(field.physical, field.width, *field.logical);
}

}  // namespace

std::string_view to_string(const physical_kind kind) noexcept {
  switch (kind) {
    case physical_kind::unsigned_integer:
      return "uint";
    case physical_kind::ascii:
      return "ascii";
    case physical_kind::reserved:
      return "reserved";
  }
  return "unknown";
}

std::string_view to_string(const logical_kind kind) noexcept {
  switch (kind) {
    case logical_kind::raw_unsigned:
      return "raw_unsigned";
    case logical_kind::timestamp_ns:
      return "timestamp_ns";
    case logical_kind::decimal:
      return "decimal";
    case logical_kind::stock_locate:
      return "stock_locate";
    case logical_kind::tracking_number:
      return "tracking_number";
    case logical_kind::order_reference_number:
      return "order_reference_number";
    case logical_kind::match_number:
      return "match_number";
    case logical_kind::share_count:
      return "share_count";
    case logical_kind::ascii:
      return "ascii";
  }
  return "unknown";
}

bool is_cpp_keyword(const std::string_view name) noexcept {
  return std::ranges::find(cpp_keywords, name) != cpp_keywords.end();
}

bool is_valid_source_name(const std::string_view name) noexcept {
  if (name.empty() || !is_ascii_lower(name.front()) ||
      name.find("__") != std::string_view::npos || is_cpp_keyword(name)) {
    return false;
  }
  return std::ranges::all_of(name.substr(1), [](const char character) {
    return is_ascii_lower(character) || is_ascii_digit(character) ||
           character == '_';
  });
}

bool is_valid_namespace_component(const std::string_view name) noexcept {
  if (name.empty() || name.front() == '_' ||
      name.find("__") != std::string_view::npos || is_cpp_keyword(name)) {
    return false;
  }
  if (!(is_ascii_lower(name.front()) || is_ascii_upper(name.front()))) {
    return false;
  }
  return std::ranges::all_of(name.substr(1), [](const char character) {
    return is_ascii_lower(character) || is_ascii_upper(character) ||
           is_ascii_digit(character) || character == '_';
  });
}

result<resolved_schema> validate_schema(const schema_source& source) {
  if (source.format_version != 1) {
    return std::unexpected(schema_problem(
        source, source.mark, "FFSCHEMA001", "schema.format_version",
        "schema format_version must be exactly 1"));
  }
  if (!is_valid_source_name(source.name)) {
    return std::unexpected(schema_problem(
        source, source.mark, "FFSCHEMA005", "schema.name",
        "schema name must match [a-z][a-z0-9_]* and not be a C++ keyword"));
  }
  if (source.wire_endian != "big") {
    return std::unexpected(schema_problem(
        source, source.mark, "FFSCHEMA011", "schema.wire_endian",
        "wire_endian must be \"big\" in schema format version 1"));
  }
  if (source.discriminator_offset != 0 || source.discriminator_width != 1) {
    return std::unexpected(schema_problem(
        source, source.mark, "FFSCHEMA027", "schema.discriminator",
        "the schema discriminator must have offset 0 and width 1"));
  }
  if (source.messages.empty()) {
    return std::unexpected(schema_problem(
        source, source.mark, "FFSCHEMA030", "schema.messages",
        "a schema must define at least one message"));
  }

  resolved_schema resolved{
      .format_version = 1U,
      .name = source.name,
      .protocol_version = source.protocol_version,
      .document_revision = source.document_revision,
      .wire_endian = source.wire_endian,
      .discriminator_offset = 0U,
      .discriminator_width = 1U,
      .types = {},
      .messages = {},
  };

  std::map<std::string, resolved_type, std::less<>> declared_types;
  for (std::size_t index = 0; index < source.types.size(); ++index) {
    const type_source& type = source.types[index];
    const std::string object_path = type_object(index, type.name);
    if (!is_valid_source_name(type.name)) {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA005", object_path + ".name",
          "type name must match [a-z][a-z0-9_]* and not be a C++ keyword"));
    }
    if (std::ranges::find(builtin_names, type.name) != builtin_names.end()) {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA007", object_path + ".name",
          "user type '" + type.name + "' shadows a reserved built-in type"));
    }
    if (declared_types.contains(type.name)) {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA006", object_path + ".name",
          "duplicate type name '" + type.name + "'"));
    }

    physical_kind physical{};
    if (type.kind == "uint") {
      physical = physical_kind::unsigned_integer;
    } else if (type.kind == "ascii") {
      physical = physical_kind::ascii;
    } else {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA012", object_path + ".kind",
          "unsupported physical kind '" + type.kind + "'"));
    }

    if (type.width <= 0 ||
        type.width >
            static_cast<std::int64_t>(std::numeric_limits<std::uint16_t>::max())) {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA014", object_path + ".width",
          "type width must be positive and no greater than 65535"));
    }
    const auto width = static_cast<std::uint32_t>(type.width);
    if (physical == physical_kind::unsigned_integer && width != 1U &&
        width != 2U && width != 4U && width != 6U && width != 8U) {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA014", object_path + ".width",
          "unsigned integer width must be 1, 2, 4, 6, or 8 bytes"));
    }

    const std::string logical_name =
        type.logical.value_or(physical == physical_kind::unsigned_integer
                                  ? "raw_unsigned"
                                  : "ascii");
    const auto logical = parse_logical(logical_name);
    if (!logical) {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA013", object_path + ".logical",
          "unsupported logical type '" + logical_name + "'"));
    }

    std::optional<std::uint32_t> scale;
    if (*logical == logical_kind::decimal) {
      if (!type.scale || *type.scale < 0 || *type.scale > 18) {
        return std::unexpected(schema_problem(
            source, type.mark, "FFSCHEMA015", object_path + ".scale",
            "decimal types require an integer scale in [0, 18]"));
      }
      scale = static_cast<std::uint32_t>(*type.scale);
    } else if (type.scale) {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA015", object_path + ".scale",
          "scale is permitted only for the decimal logical type"));
    }

    if (!compatible(physical, width, *logical)) {
      return std::unexpected(schema_problem(
          source, type.mark, "FFSCHEMA016", object_path,
          "logical type '" + logical_name + "' is incompatible with " +
              std::string(to_string(physical)) + " width " +
              std::to_string(width)));
    }

    resolved_type result_type{
        .name = type.name,
        .physical = physical,
        .width = width,
        .logical = *logical,
        .scale = scale,
    };
    declared_types.emplace(result_type.name, result_type);
    resolved.types.push_back(std::move(result_type));
  }

  std::set<std::string, std::less<>> message_names;
  std::set<std::uint8_t> discriminators;
  for (std::size_t message_index = 0;
       message_index < source.messages.size(); ++message_index) {
    const message_source& message = source.messages[message_index];
    const std::string message_path =
        message_object(message_index, message.name);
    if (!is_valid_source_name(message.name)) {
      return std::unexpected(schema_problem(
          source, message.mark, "FFSCHEMA005", message_path + ".name",
          "message name must match [a-z][a-z0-9_]* and not be a C++ keyword"));
    }
    if (!message_names.insert(message.name).second) {
      return std::unexpected(schema_problem(
          source, message.mark, "FFSCHEMA008", message_path + ".name",
          "duplicate message name '" + message.name + "'"));
    }
    if (message.type.size() != 1U ||
        static_cast<unsigned char>(message.type.front()) < 0x21U ||
        static_cast<unsigned char>(message.type.front()) > 0x7eU) {
      return std::unexpected(schema_problem(
          source, message.mark, "FFSCHEMA017", message_path + ".type",
          "message type must be exactly one printable ASCII byte"));
    }
    const auto discriminator =
        static_cast<std::uint8_t>(static_cast<unsigned char>(message.type[0]));
    if (!discriminators.insert(discriminator).second) {
      return std::unexpected(schema_problem(
          source, message.mark, "FFSCHEMA009", message_path + ".type",
          "duplicate message discriminator '" + message.type + "'"));
    }
    if (message.size <= 0 ||
        message.size >
            static_cast<std::int64_t>(std::numeric_limits<std::uint16_t>::max())) {
      return std::unexpected(schema_problem(
          source, message.mark, "FFSCHEMA018", message_path + ".size",
          "message size must be in [1, 65535]"));
    }
    if (message.fields.empty()) {
      return std::unexpected(schema_problem(
          source, message.mark, "FFSCHEMA031", message_path + ".fields",
          "a message must define at least one field"));
    }

    resolved_message result_message{
        .name = message.name,
        .discriminator = discriminator,
        .size = static_cast<std::uint32_t>(message.size),
        .fields = {},
    };
    std::set<std::string, std::less<>> field_names;
    std::size_t discriminator_count = 0U;
    for (std::size_t field_index = 0; field_index < message.fields.size();
         ++field_index) {
      const field_source& field = message.fields[field_index];
      const std::string object_path =
          field_object(message_path, field_index, field.name);
      if (!is_valid_source_name(field.name)) {
        return std::unexpected(schema_problem(
            source, field.mark, "FFSCHEMA005", object_path + ".name",
            "field name must match [a-z][a-z0-9_]* and not be a C++ keyword"));
      }
      if (!field_names.insert(field.name).second) {
        return std::unexpected(schema_problem(
            source, field.mark, "FFSCHEMA010", object_path + ".name",
            "duplicate field name '" + field.name + "'"));
      }
      if (field.offset < 0 ||
          field.offset >
              static_cast<std::int64_t>(
                  std::numeric_limits<std::uint32_t>::max())) {
        return std::unexpected(schema_problem(
            source, field.mark, "FFSCHEMA021", object_path + ".offset",
            "field offset must be non-negative and representable in 32 bits"));
      }
      if (field.width <= 0 ||
          field.width >
              static_cast<std::int64_t>(
                  std::numeric_limits<std::uint16_t>::max())) {
        return std::unexpected(schema_problem(
            source, field.mark, "FFSCHEMA014", object_path + ".width",
            "field width must be in [1, 65535]"));
      }
      const auto offset = static_cast<std::uint32_t>(field.offset);
      const auto width = static_cast<std::uint32_t>(field.width);

      std::optional<type_description> description =
          builtin_type(field.type, width);
      if (!description) {
        const auto found = declared_types.find(field.type);
        if (found == declared_types.end()) {
          return std::unexpected(schema_problem(
              source, field.mark, "FFSCHEMA019", object_path + ".type",
              "field references undeclared type '" + field.type + "'"));
        }
        description = type_description{
            .physical = found->second.physical,
            .width = found->second.width,
            .logical = found->second.logical,
            .scale = found->second.scale,
            .variable_width = false,
        };
      }
      if (!description->variable_width && width != description->width) {
        return std::unexpected(schema_problem(
            source, field.mark, "FFSCHEMA020", object_path + ".width",
            "field width " + std::to_string(width) +
                " disagrees with type width " +
                std::to_string(description->width)));
      }

      for (const std::string& allowed : field.allowed) {
        if (allowed.size() != static_cast<std::size_t>(width)) {
          return std::unexpected(schema_problem(
              source, field.mark, "FFSCHEMA034", object_path + ".allowed",
              "each allowed value must contain exactly " +
                  std::to_string(width) + " bytes"));
        }
      }

      const bool is_discriminator = field.role.has_value();
      if (field.role && *field.role != "discriminator") {
        return std::unexpected(schema_problem(
            source, field.mark, "FFSCHEMA035", object_path + ".role",
            "field role must be \"discriminator\" when present"));
      }
      if (is_discriminator) {
        ++discriminator_count;
        if (!field.value) {
          return std::unexpected(schema_problem(
              source, field.mark, "FFSCHEMA035", object_path + ".value",
              "the discriminator field requires value"));
        }
        if (offset != 0U || width != 1U) {
          return std::unexpected(schema_problem(
              source, field.mark, "FFSCHEMA027", object_path,
              "the discriminator field must have offset 0 and width 1"));
        }
        if (description->physical != physical_kind::ascii) {
          return std::unexpected(schema_problem(
              source, field.mark, "FFSCHEMA033", object_path + ".type",
              "the discriminator field must use an ASCII physical type"));
        }
        if (*field.value != message.type) {
          return std::unexpected(schema_problem(
              source, field.mark, "FFSCHEMA028", object_path + ".value",
              "discriminator value '" + *field.value +
                  "' disagrees with message type '" + message.type + "'"));
        }
      } else if (field.value) {
        return std::unexpected(schema_problem(
            source, field.mark, "FFSCHEMA035", object_path + ".value",
            "value is permitted only on the discriminator field"));
      }

      result_message.fields.push_back(resolved_field{
          .name = field.name,
          .type_name = field.type,
          .physical = description->physical,
          .logical = description->logical,
          .offset = offset,
          .width = width,
          .scale = description->scale,
          .discriminator = is_discriminator,
          .projectable = !is_discriminator &&
              description->physical != physical_kind::reserved,
      });
    }

    if (discriminator_count != 1U) {
      return std::unexpected(schema_problem(
          source, message.mark, "FFSCHEMA026", message_path + ".fields",
          "a message must define exactly one discriminator field"));
    }

    std::ranges::sort(
        result_message.fields, {},
        [](const resolved_field& field) {
          return std::pair{field.offset, field.name};
        });
    std::uint32_t covered_until = 0U;
    for (const resolved_field& field : result_message.fields) {
      const auto original_field = std::ranges::find(
          message.fields, field.name, &field_source::name);
      const source_mark& field_mark =
          original_field == message.fields.end() ? message.mark
                                                 : original_field->mark;
      if (field.offset < covered_until) {
        return std::unexpected(schema_problem(
            source, field_mark, "FFSCHEMA023",
            message_path + ".fields." + field.name,
            "field overlaps a preceding field"));
      }
      if (field.offset > covered_until) {
        return std::unexpected(schema_problem(
            source, field_mark, "FFSCHEMA024",
            message_path + ".fields." + field.name,
            "undeclared byte gap begins at offset " +
                std::to_string(covered_until),
            "declare the range explicitly with a field of type reserved"));
      }
      const std::uint64_t end = static_cast<std::uint64_t>(field.offset) +
                                static_cast<std::uint64_t>(field.width);
      if (end > result_message.size) {
        return std::unexpected(schema_problem(
            source, field_mark, "FFSCHEMA022",
            message_path + ".fields." + field.name,
            "field ends at byte " + std::to_string(end) +
                ", beyond declared size " +
                std::to_string(result_message.size)));
      }
      covered_until = static_cast<std::uint32_t>(end);
    }
    if (covered_until != result_message.size) {
      return std::unexpected(schema_problem(
          source, message.mark, "FFSCHEMA025", message_path + ".fields",
          "declared fields end at byte " + std::to_string(covered_until) +
              ", before declared size " +
              std::to_string(result_message.size),
          "declare the trailing range explicitly with a field of type reserved"));
    }

    resolved.messages.push_back(std::move(result_message));
  }

  std::ranges::sort(resolved.types, {}, &resolved_type::name);
  std::ranges::sort(resolved.messages, {},
                    &resolved_message::discriminator);
  return resolved;
}

result<resolved_pipeline> validate_pipeline(const pipeline_source& source,
                                            const resolved_schema& schema) {
  if (source.format_version != 1) {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE001", "pipeline.format_version",
        "pipeline format_version must be exactly 1"));
  }
  if (!is_valid_source_name(source.name)) {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE005", "pipeline.name",
        "pipeline name must match [a-z][a-z0-9_]* and not be a C++ keyword"));
  }
  if (!is_valid_source_name(source.schema)) {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE005", "pipeline.schema",
        "schema reference must match [a-z][a-z0-9_]*"));
  }
  if (source.schema != schema.name) {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE007", "pipeline.schema",
        "pipeline schema '" + source.schema +
            "' does not match parsed schema '" + schema.name + "'"));
  }
  if (source.profile != "portable_checked") {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE008", "pipeline.profile",
        "unsupported profile '" + source.profile + "'",
        "v0.1 supports only portable_checked"));
  }
  if (source.unknown_messages != "error" &&
      source.unknown_messages != "skip") {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE009", "pipeline.unknown_messages",
        "unknown_messages must be \"error\" or \"skip\""));
  }
  if (source.unselected_messages != "skip") {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE009", "pipeline.unselected_messages",
        "unselected_messages must be \"skip\" in v0.1"));
  }
  if (source.cpp_namespace.empty()) {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE006", "pipeline.namespace",
        "namespace must contain at least one component"));
  }
  std::size_t component_start = 0U;
  while (component_start <= source.cpp_namespace.size()) {
    const std::size_t separator =
        source.cpp_namespace.find("::", component_start);
    const std::size_t component_end =
        separator == std::string::npos ? source.cpp_namespace.size()
                                       : separator;
    const std::string_view component{source.cpp_namespace.data() +
                                         component_start,
                                     component_end - component_start};
    if (!is_valid_namespace_component(component)) {
      return std::unexpected(pipeline_problem(
          source, source.mark, "FFPIPE006", "pipeline.namespace",
          "invalid or reserved C++ namespace component '" +
              std::string(component) + "'"));
    }
    if (separator == std::string::npos) {
      break;
    }
    component_start = separator + 2U;
  }
  if (source.projections.empty()) {
    return std::unexpected(pipeline_problem(
        source, source.mark, "FFPIPE010", "pipeline.emit",
        "a pipeline must define at least one emit table"));
  }

  std::map<std::uint8_t, const resolved_message*> messages;
  for (const resolved_message& message : schema.messages) {
    messages.emplace(message.discriminator, &message);
  }

  resolved_pipeline resolved{
      .format_version = 1U,
      .name = source.name,
      .cpp_namespace = source.cpp_namespace,
      .schema = source.schema,
      .profile = source.profile,
      .variant_id = "portable_checked.v1",
      .unknown_messages = source.unknown_messages,
      .unselected_messages = source.unselected_messages,
      .projections = {},
  };
  std::set<std::uint8_t> selected_sources;
  std::set<std::string, std::less<>> event_names;
  for (std::size_t index = 0; index < source.projections.size(); ++index) {
    const projection_source& projection = source.projections[index];
    const std::string object_path = projection_object(index, projection.event);
    if (projection.source.size() != 1U) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE011", object_path + ".source",
          "emit source must be exactly one byte"));
    }
    const auto discriminator = static_cast<std::uint8_t>(
        static_cast<unsigned char>(projection.source[0]));
    const auto message = messages.find(discriminator);
    if (message == messages.end()) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE012", object_path + ".source",
          "unknown source message '" + projection.source + "'"));
    }
    if (!selected_sources.insert(discriminator).second) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE013", object_path + ".source",
          "duplicate source selection '" + projection.source + "'"));
    }
    if (!is_valid_source_name(projection.event)) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE005", object_path + ".event",
          "event name must match [a-z][a-z0-9_]* and not be a C++ keyword"));
    }
    if (std::ranges::find(generated_pipeline_declarations, projection.event) !=
        generated_pipeline_declarations.end()) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE021", object_path + ".event",
          "event name '" + projection.event +
              "' collides with a generated pipeline declaration"));
    }
    if (!event_names.insert(projection.event).second) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE014", object_path + ".event",
          "duplicate event name '" + projection.event + "'"));
    }
    if (projection.fields.empty()) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE015", object_path + ".fields",
          "emit fields must not be empty"));
    }

    const bool contains_wildcard =
        std::ranges::find(projection.fields, "*") != projection.fields.end();
    if (contains_wildcard &&
        (projection.fields.size() != 1U || projection.fields.front() != "*")) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE016", object_path + ".fields",
          "wildcard must appear alone as fields = [\"*\"]"));
    }

    resolved_projection result_projection{
        .source_discriminator = discriminator,
        .source_message = message->second->name,
        .event = projection.event,
        .fields = {},
    };
    if (contains_wildcard) {
      for (const resolved_field& field : message->second->fields) {
        if (field.projectable) {
          result_projection.fields.push_back(field);
        }
      }
    } else {
      std::set<std::string, std::less<>> projected_names;
      for (std::size_t field_index = 0;
           field_index < projection.fields.size(); ++field_index) {
        const std::string& field_name = projection.fields[field_index];
        const source_mark& field_mark =
            field_index < projection.field_marks.size()
                ? projection.field_marks[field_index]
                : projection.mark;
        if (!projected_names.insert(field_name).second) {
          return std::unexpected(pipeline_problem(
              source, field_mark, "FFPIPE017", object_path + ".fields",
              "duplicate projected field '" + field_name + "'"));
        }
        const auto field = std::ranges::find(
            message->second->fields, field_name, &resolved_field::name);
        if (field == message->second->fields.end()) {
          return std::unexpected(pipeline_problem(
              source, field_mark, "FFPIPE018", object_path + ".fields",
              "message '" + message->second->name +
                  "' has no field named '" + field_name + "'"));
        }
        if (!field->projectable) {
          return std::unexpected(pipeline_problem(
              source, field_mark, "FFPIPE019", object_path + ".fields",
              "field '" + field_name +
                  "' is a discriminator or reserved field and cannot be "
                  "projected"));
        }
        if (!has_cpp_representation(*field)) {
          return std::unexpected(pipeline_problem(
              source, field_mark, "FFPIPE020", object_path + ".fields",
              "field '" + field_name +
                  "' has no C++20 value representation"));
        }
        result_projection.fields.push_back(*field);
      }
    }
    if (result_projection.fields.empty()) {
      return std::unexpected(pipeline_problem(
          source, projection.mark, "FFPIPE015", object_path + ".fields",
          "emit resolves to no projectable fields"));
    }
    for (const resolved_field& field : result_projection.fields) {
      if (field.name == "source_discriminator" ||
          field.name == "event_name" ||
          field.name == result_projection.event) {
        return std::unexpected(pipeline_problem(
            source, projection.mark, "FFPIPE021", object_path + ".fields",
            "projected field name '" + field.name +
                "' collides with a generated event declaration"));
      }
    }
    resolved.projections.push_back(std::move(result_projection));
  }

  std::ranges::sort(resolved.projections, {},
                    &resolved_projection::source_discriminator);
  return resolved;
}

}  // namespace feedforge::compiler
