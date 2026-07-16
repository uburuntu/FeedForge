#include "emit_cpp.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "sha256.hpp"

namespace feedforge::compiler {
namespace {

inline constexpr std::uint32_t required_runtime_api_version = 1U;

[[nodiscard]] diagnostic emitter_problem(
    std::string object_path, std::string message,
    std::optional<std::string> hint = std::nullopt) {
  return make_diagnostic("FFEMIT001", {}, source_mark{},
                         std::move(object_path), std::move(message),
                         std::move(hint));
}

[[nodiscard]] bool is_lower_hex_fingerprint(const std::string_view value) {
  if (value.size() != 64U) {
    return false;
  }
  return std::ranges::all_of(value, [](const char character) {
    return (character >= '0' && character <= '9') ||
           (character >= 'a' && character <= 'f');
  });
}

[[nodiscard]] bool is_generated_declaration(
    const std::string_view identifier) {
  constexpr std::array declarations{
      std::string_view{"basic_decoder"},
      std::string_view{"decoder"},
      std::string_view{"pipeline_metadata"},
      std::string_view{"sink_for_all_selected_events"},
  };
  return std::ranges::find(declarations, identifier) != declarations.end();
}

[[nodiscard]] bool fields_equal(const ffir_field_v1& left,
                                const ffir_field_v1& right) {
  return left.name == right.name && left.type_name == right.type_name &&
         left.physical == right.physical && left.logical == right.logical &&
         left.offset == right.offset && left.width == right.width &&
         left.scale == right.scale &&
         left.discriminator == right.discriminator &&
         left.projectable == right.projectable;
}

[[nodiscard]] const ffir_message_v1* find_message(
    const ffir_v1& ir, const std::uint8_t discriminator) {
  const auto message =
      std::ranges::find(ir.schema.messages, discriminator,
                        &ffir_message_v1::discriminator);
  return message == ir.schema.messages.end() ? nullptr : &*message;
}

[[nodiscard]] std::optional<std::string> unsigned_cpp_type(
    const std::uint32_t width) {
  switch (width) {
    case 1U:
      return "std::uint8_t";
    case 2U:
      return "std::uint16_t";
    case 4U:
      return "std::uint32_t";
    case 6U:
    case 8U:
      return "std::uint64_t";
    default:
      return std::nullopt;
  }
}

[[nodiscard]] std::optional<std::string> cpp_type(
    const ffir_field_v1& field) {
  if (!field.logical) {
    return std::nullopt;
  }

  switch (*field.logical) {
    case logical_kind::raw_unsigned:
      if (field.physical != physical_kind::unsigned_integer || field.scale) {
        return std::nullopt;
      }
      return unsigned_cpp_type(field.width);
    case logical_kind::ascii:
      if (field.physical != physical_kind::ascii || field.width == 0U ||
          field.scale) {
        return std::nullopt;
      }
      return "feedforge::ascii<" + std::to_string(field.width) + "U>";
    case logical_kind::timestamp_ns:
      if (field.physical != physical_kind::unsigned_integer ||
          field.width != 6U || field.scale) {
        return std::nullopt;
      }
      return "feedforge::timestamp_ns";
    case logical_kind::decimal:
      if (field.physical != physical_kind::unsigned_integer ||
          (field.width != 4U && field.width != 8U) || !field.scale ||
          *field.scale > 18U) {
        return std::nullopt;
      }
      return "feedforge::decimal<" +
             std::string(field.width == 4U ? "std::uint32_t"
                                          : "std::uint64_t") +
             ", " + std::to_string(*field.scale) + ">";
    case logical_kind::stock_locate:
      if (field.physical != physical_kind::unsigned_integer ||
          field.width != 2U || field.scale) {
        return std::nullopt;
      }
      return "feedforge::stock_locate";
    case logical_kind::tracking_number:
      if (field.physical != physical_kind::unsigned_integer ||
          field.width != 2U || field.scale) {
        return std::nullopt;
      }
      return "feedforge::tracking_number";
    case logical_kind::order_reference_number:
      if (field.physical != physical_kind::unsigned_integer ||
          field.width != 8U || field.scale) {
        return std::nullopt;
      }
      return "feedforge::order_reference_number";
    case logical_kind::match_number:
      if (field.physical != physical_kind::unsigned_integer ||
          field.width != 8U || field.scale) {
        return std::nullopt;
      }
      return "feedforge::match_number";
    case logical_kind::share_count:
      if (field.physical != physical_kind::unsigned_integer ||
          field.width != 4U || field.scale) {
        return std::nullopt;
      }
      return "feedforge::share_count";
  }
  return std::nullopt;
}

[[nodiscard]] result<void> validate_ir(const ffir_v1& ir) {
  if (ir.format_version != ffir_format_version) {
    return std::unexpected(emitter_problem(
        "ffir.format_version",
        "unsupported FFIR format version " +
            std::to_string(ir.format_version)));
  }
  if (ir.generator_version.empty()) {
    return std::unexpected(emitter_problem(
        "ffir.generator_version", "generator version must not be empty"));
  }
  if (!is_lower_hex_fingerprint(ir.schema.fingerprint)) {
    return std::unexpected(emitter_problem(
        "ffir.schema.fingerprint",
        "schema fingerprint must be 64 lowercase hexadecimal characters"));
  }
  if (!is_lower_hex_fingerprint(ir.pipeline.fingerprint)) {
    return std::unexpected(emitter_problem(
        "ffir.pipeline.fingerprint",
        "pipeline fingerprint must be 64 lowercase hexadecimal characters"));
  }
  if (sha256_hex(canonical_schema_semantics_json(ir)) !=
      ir.schema.fingerprint) {
    return std::unexpected(emitter_problem(
        "ffir.schema.fingerprint",
        "schema fingerprint does not match resolved FFIR semantics"));
  }
  if (sha256_hex(canonical_pipeline_semantics_json(ir)) !=
      ir.pipeline.fingerprint) {
    return std::unexpected(emitter_problem(
        "ffir.pipeline.fingerprint",
        "pipeline fingerprint does not match resolved FFIR semantics"));
  }
  if (ir.schema.wire_endian != "big" ||
      ir.schema.discriminator_offset != 0U ||
      ir.schema.discriminator_width != 1U) {
    return std::unexpected(emitter_problem(
        "ffir.schema.wire",
        "portable_checked.v1 requires a one-byte discriminator at offset zero "
        "and big-endian wire values"));
  }
  if (ir.pipeline.profile != "portable_checked" ||
      ir.pipeline.variant_id != "portable_checked.v1") {
    return std::unexpected(emitter_problem(
        "ffir.pipeline.profile",
        "the C++20 backend supports only portable_checked.v1"));
  }
  if (ir.pipeline.unknown_messages != "error" &&
      ir.pipeline.unknown_messages != "skip") {
    return std::unexpected(emitter_problem(
        "ffir.pipeline.unknown_messages",
        "unknown message policy must be error or skip"));
  }
  if (ir.pipeline.unselected_messages != "skip") {
    return std::unexpected(emitter_problem(
        "ffir.pipeline.unselected_messages",
        "unselected message policy must be skip in v0.1"));
  }
  if (ir.pipeline.cpp_namespace.empty()) {
    return std::unexpected(emitter_problem(
        "ffir.pipeline.namespace", "generated namespace must not be empty"));
  }

  std::size_t component_start = 0U;
  while (component_start <= ir.pipeline.cpp_namespace.size()) {
    const std::size_t separator =
        ir.pipeline.cpp_namespace.find("::", component_start);
    const std::size_t component_end =
        separator == std::string::npos ? ir.pipeline.cpp_namespace.size()
                                       : separator;
    const std::string_view component{
        ir.pipeline.cpp_namespace.data() + component_start,
        component_end - component_start};
    if (!is_valid_namespace_component(component)) {
      return std::unexpected(emitter_problem(
          "ffir.pipeline.namespace",
          "generated namespace contains invalid component '" +
              std::string(component) + "'"));
    }
    if (separator == std::string::npos) {
      break;
    }
    component_start = separator + 2U;
  }

  if (ir.schema.messages.empty()) {
    return std::unexpected(emitter_problem(
        "ffir.schema.messages", "FFIR must contain at least one known message"));
  }
  std::optional<std::uint8_t> previous_message_discriminator;
  for (std::size_t index = 0U; index < ir.schema.messages.size(); ++index) {
    const ffir_message_v1& message = ir.schema.messages[index];
    const std::string object_path =
        "ffir.schema.messages[" + std::to_string(index) + "]";
    if (!is_valid_source_name(message.name)) {
      return std::unexpected(emitter_problem(
          object_path + ".name",
          "known message name is not a safe C++ identifier"));
    }
    if (message.discriminator < 0x21U || message.discriminator > 0x7eU) {
      return std::unexpected(emitter_problem(
          object_path + ".discriminator",
          "known message discriminator must be printable ASCII"));
    }
    if (message.size == 0U ||
        message.size > std::numeric_limits<std::uint16_t>::max()) {
      return std::unexpected(emitter_problem(
          object_path + ".size",
          "known message size must fit a nonzero uint16_t"));
    }
    if (previous_message_discriminator &&
        *previous_message_discriminator >= message.discriminator) {
      return std::unexpected(emitter_problem(
          object_path + ".discriminator",
          "known messages must be unique and sorted by unsigned "
          "discriminator"));
    }
    previous_message_discriminator = message.discriminator;
  }

  if (ir.pipeline.events.empty()) {
    return std::unexpected(emitter_problem(
        "ffir.pipeline.events", "FFIR must contain at least one selected event"));
  }
  std::set<std::string, std::less<>> event_names;
  std::optional<std::uint8_t> previous_event_discriminator;
  for (std::size_t event_index = 0U;
       event_index < ir.pipeline.events.size(); ++event_index) {
    const ffir_event_v1& event = ir.pipeline.events[event_index];
    const std::string event_path =
        "ffir.pipeline.events[" + std::to_string(event_index) + "]";
    if (!is_valid_source_name(event.event)) {
      return std::unexpected(emitter_problem(
          event_path + ".event",
          "event name is not a safe C++ identifier"));
    }
    if (is_generated_declaration(event.event)) {
      return std::unexpected(emitter_problem(
          event_path + ".event",
          "event name '" + event.event +
              "' collides with a generated declaration"));
    }
    if (!event_names.insert(event.event).second) {
      return std::unexpected(emitter_problem(
          event_path + ".event", "event names must be unique"));
    }
    if (previous_event_discriminator &&
        *previous_event_discriminator >= event.source_discriminator) {
      return std::unexpected(emitter_problem(
          event_path + ".source_discriminator",
          "selected events must be unique and sorted by unsigned "
          "discriminator"));
    }
    previous_event_discriminator = event.source_discriminator;

    const ffir_message_v1* message =
        find_message(ir, event.source_discriminator);
    if (message == nullptr) {
      return std::unexpected(emitter_problem(
          event_path + ".source_discriminator",
          "selected event does not name a known message discriminator"));
    }
    if (event.source_message != message->name) {
      return std::unexpected(emitter_problem(
          event_path + ".source_message",
          "selected event source message does not match its discriminator"));
    }
    if (event.fields.empty()) {
      return std::unexpected(emitter_problem(
          event_path + ".fields",
          "selected event must contain at least one projected field"));
    }

    std::set<std::string, std::less<>> field_names;
    for (std::size_t field_index = 0U; field_index < event.fields.size();
         ++field_index) {
      const ffir_field_v1& field = event.fields[field_index];
      const std::string field_path =
          event_path + ".fields[" + std::to_string(field_index) + "]";
      if (!is_valid_source_name(field.name)) {
        return std::unexpected(emitter_problem(
            field_path + ".name",
            "projected field name is not a safe C++ identifier"));
      }
      if (field.name == "source_discriminator" ||
          field.name == "event_name" || field.name == event.event) {
        return std::unexpected(emitter_problem(
            field_path + ".name",
            "projected field name '" + field.name +
                "' collides with a generated event declaration"));
      }
      if (!field_names.insert(field.name).second) {
        return std::unexpected(emitter_problem(
            field_path + ".name",
            "projected field names must be unique within an event"));
      }
      if (!field.projectable || field.discriminator ||
          field.physical == physical_kind::reserved) {
        return std::unexpected(emitter_problem(
            field_path,
            "event contains a discriminator or non-projectable field"));
      }
      if (field.offset > message->size ||
          field.width > message->size - field.offset) {
        return std::unexpected(emitter_problem(
            field_path,
            "projected field extends beyond its known message size"));
      }
      if (!cpp_type(field)) {
        return std::unexpected(emitter_problem(
            field_path,
            "projected field has no exact C++20 value representation"));
      }

      const auto source_field =
          std::ranges::find(message->fields, field.name, &ffir_field_v1::name);
      if (source_field == message->fields.end() ||
          !fields_equal(field, *source_field)) {
        return std::unexpected(emitter_problem(
            field_path,
            "projected field does not exactly match its resolved schema field"));
      }
    }
  }
  return {};
}

[[nodiscard]] std::string escape_cpp_string(const std::string_view value) {
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
      case '\a':
        escaped += "\\a";
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
      case '\v':
        escaped += "\\v";
        break;
      default:
        if (character >= 0x20U && character <= 0x7eU) {
          escaped.push_back(static_cast<char>(character));
        } else {
          escaped.push_back('\\');
          escaped.push_back(
              static_cast<char>('0' + ((character >> 6U) & 0x07U)));
          escaped.push_back(
              static_cast<char>('0' + ((character >> 3U) & 0x07U)));
          escaped.push_back(static_cast<char>('0' + (character & 0x07U)));
        }
        break;
    }
  }
  return escaped;
}

[[nodiscard]] std::string escape_comment(const std::string_view value) {
  constexpr std::string_view hexadecimal{"0123456789ABCDEF"};
  std::string escaped;
  escaped.reserve(value.size());
  for (const char raw_character : value) {
    const auto character = static_cast<unsigned char>(raw_character);
    if (character >= 0x20U && character <= 0x7eU) {
      escaped.push_back(static_cast<char>(character));
    } else {
      escaped += "\\x";
      escaped.push_back(hexadecimal[character >> 4U]);
      escaped.push_back(hexadecimal[character & 0x0fU]);
    }
  }
  return escaped;
}

[[nodiscard]] std::string byte_literal(const std::uint8_t value) {
  constexpr std::string_view hexadecimal{"0123456789ABCDEF"};
  std::string result{"0x00U"};
  result[2] = hexadecimal[(value >> 4U) & 0x0fU];
  result[3] = hexadecimal[value & 0x0fU];
  return result;
}

[[nodiscard]] std::string include_guard(const ffir_v1& ir) {
  std::string guard{"FEEDFORGE_GENERATED_"};
  guard.reserve(guard.size() + ir.schema.fingerprint.size() +
                ir.pipeline.fingerprint.size() + 5U);
  const auto append_fingerprint = [&](const std::string_view fingerprint) {
    for (const char character : fingerprint) {
      guard.push_back(character >= 'a' && character <= 'f'
                          ? static_cast<char>(character - 'a' + 'A')
                          : character);
    }
  };
  append_fingerprint(ir.schema.fingerprint);
  guard.push_back('_');
  append_fingerprint(ir.pipeline.fingerprint);
  guard += "_HPP";
  return guard;
}

void append_line(std::string& output, const std::size_t indentation,
                 const std::string_view text = {}) {
  output.append(indentation * 2U, ' ');
  output += text;
  output.push_back('\n');
}

[[nodiscard]] std::size_t message_index(const ffir_v1& ir,
                                        const std::uint8_t discriminator) {
  const auto message =
      std::ranges::find(ir.schema.messages, discriminator,
                        &ffir_message_v1::discriminator);
  return static_cast<std::size_t>(
      std::distance(ir.schema.messages.begin(), message));
}

[[nodiscard]] std::string load_expression(const ffir_field_v1& field) {
  return "Implementation::template load_unsigned<" +
         std::to_string(field.width) + "U>(payload.data() + " +
         std::to_string(field.offset) + "U)";
}

void append_field_load(std::string& output, const ffir_field_v1& field) {
  if (field.physical == physical_kind::ascii) {
    append_line(output, 4U,
                "std::memcpy(event." + field.name +
                    ".raw.data(), payload.data() + " +
                    std::to_string(field.offset) + "U, " +
                    std::to_string(field.width) + "U);");
    return;
  }

  const std::string load = load_expression(field);
  const std::string type = *cpp_type(field);
  switch (*field.logical) {
    case logical_kind::raw_unsigned:
      append_line(output, 4U,
                  "event." + field.name + " = static_cast<" + type + ">(" +
                      load + ");");
      return;
    case logical_kind::timestamp_ns:
      append_line(output, 4U,
                  "event." + field.name + " = feedforge::timestamp_ns{" +
                      load + "};");
      return;
    case logical_kind::decimal: {
      const std::string representation =
          field.width == 4U ? "std::uint32_t" : "std::uint64_t";
      append_line(output, 4U,
                  "event." + field.name + " = " + type + "{static_cast<" +
                      representation + ">(" + load + ")};");
      return;
    }
    case logical_kind::stock_locate:
      append_line(output, 4U,
                  "event." + field.name +
                      " = feedforge::stock_locate{static_cast<std::uint16_t>(" +
                      load + ")};");
      return;
    case logical_kind::tracking_number:
      append_line(
          output, 4U,
          "event." + field.name +
              " = feedforge::tracking_number{static_cast<std::uint16_t>(" +
              load + ")};");
      return;
    case logical_kind::order_reference_number:
      append_line(
          output, 4U,
          "event." + field.name +
              " = feedforge::order_reference_number{static_cast<std::uint64_t>(" +
              load + ")};");
      return;
    case logical_kind::match_number:
      append_line(output, 4U,
                  "event." + field.name +
                      " = feedforge::match_number{static_cast<std::uint64_t>(" +
                      load + ")};");
      return;
    case logical_kind::share_count:
      append_line(output, 4U,
                  "event." + field.name +
                      " = feedforge::share_count{static_cast<std::uint32_t>(" +
                      load + ")};");
      return;
    case logical_kind::ascii:
      return;
  }
}

void append_outcome(std::string& output, const std::size_t indentation,
                    const std::string_view status,
                    const std::string_view message_type,
                    const std::string_view expected_size,
                    const std::string_view actual_size) {
  append_line(output, indentation, "return feedforge::decode_outcome{");
  append_line(output, indentation + 1U,
              "feedforge::decode_status::" + std::string(status) + ",");
  append_line(output, indentation + 1U,
              std::string(message_type) + ",");
  append_line(output, indentation + 1U,
              std::string(expected_size) + ",");
  append_line(output, indentation + 1U,
              std::string(actual_size) + ",");
  append_line(output, indentation, "};");
}

void append_header(std::string& output, const ffir_v1& ir) {
  append_line(output, 0U,
              "// Generated by FeedForge compiler " +
                  escape_comment(ir.generator_version) + ".");
  append_line(output, 0U, "// DO NOT EDIT.");
  append_line(output, 0U,
              "// FFIR format version: " +
                  std::to_string(ir.format_version));
  append_line(output, 0U,
              "// Schema: " + escape_comment(ir.schema.name) +
                  " (protocol " +
                  escape_comment(ir.schema.protocol_version) +
                  ", document revision " +
                  escape_comment(ir.schema.document_revision) + ")");
  append_line(output, 0U,
              "// Schema fingerprint: " + ir.schema.fingerprint);
  append_line(output, 0U,
              "// Pipeline: " + escape_comment(ir.pipeline.name));
  append_line(output, 0U,
              "// Pipeline fingerprint: " + ir.pipeline.fingerprint);
  append_line(output, 0U,
              "// Profile/variant: " +
                  escape_comment(ir.pipeline.profile) + "/" +
                  escape_comment(ir.pipeline.variant_id));
  append_line(output, 0U);

  const std::string guard = include_guard(ir);
  append_line(output, 0U, "#ifndef " + guard);
  append_line(output, 0U, "#define " + guard);
  append_line(output, 0U);
  append_line(output, 0U, "#include <array>");
  append_line(output, 0U, "#include <concepts>");
  append_line(output, 0U, "#include <cstddef>");
  append_line(output, 0U, "#include <cstdint>");
  append_line(output, 0U, "#include <cstring>");
  append_line(output, 0U, "#include <span>");
  append_line(output, 0U, "#include <string_view>");
  append_line(output, 0U, "#include <type_traits>");
  append_line(output, 0U);
  append_line(output, 0U, "#include <feedforge/flow.hpp>");
  append_line(output, 0U,
              "#include <feedforge/framing/binary_file.hpp>");
  append_line(output, 0U, "#include <feedforge/profile/concepts.hpp>");
  append_line(output, 0U,
              "#include <feedforge/profile/portable_checked.hpp>");
  append_line(output, 0U, "#include <feedforge/result.hpp>");
  append_line(output, 0U, "#include <feedforge/types/ascii.hpp>");
  append_line(output, 0U, "#include <feedforge/types/decimal.hpp>");
  append_line(output, 0U, "#include <feedforge/types/identifiers.hpp>");
  append_line(output, 0U, "#include <feedforge/types/timestamp.hpp>");
  append_line(output, 0U, "#include <feedforge/version.hpp>");
  append_line(output, 0U,
              "#include <feedforge/wire/load_big_endian.hpp>");
  append_line(output, 0U);
  append_line(output, 0U,
              "namespace " + ir.pipeline.cpp_namespace + " {");
  append_line(output, 0U);
}

void append_metadata(std::string& output, const ffir_v1& ir) {
  append_line(output, 0U, "struct pipeline_metadata {");
  append_line(output, 1U, "struct known_message {");
  append_line(output, 2U, "std::byte discriminator{};");
  append_line(output, 2U, "std::uint16_t size{};");
  append_line(output, 1U, "};");
  append_line(output, 0U);
  append_line(output, 1U,
              "static constexpr std::uint32_t required_runtime_api_version{" +
                  std::to_string(required_runtime_api_version) + "U};");
  append_line(output, 1U,
              "static constexpr std::uint32_t ffir_format_version{" +
                  std::to_string(ir.format_version) + "U};");
  append_line(output, 1U,
              "static constexpr std::string_view generator_version{\"" +
                  escape_cpp_string(ir.generator_version) + "\"};");
  append_line(output, 1U,
              "static constexpr std::string_view schema_fingerprint{\"" +
                  escape_cpp_string(ir.schema.fingerprint) + "\"};");
  append_line(output, 1U,
              "static constexpr std::string_view pipeline_fingerprint{\"" +
                  escape_cpp_string(ir.pipeline.fingerprint) + "\"};");
  append_line(output, 1U,
              "static constexpr std::string_view profile_name{\"" +
                  escape_cpp_string(ir.pipeline.profile) + "\"};");
  append_line(output, 1U,
              "static constexpr std::string_view profile_variant_id{\"" +
                  escape_cpp_string(ir.pipeline.variant_id) + "\"};");
  append_line(output, 1U,
              "static constexpr std::array<known_message, " +
                  std::to_string(ir.schema.messages.size()) +
                  "U> known_messages{{");
  for (const ffir_message_v1& message : ir.schema.messages) {
    append_line(output, 2U,
                "{std::byte{" + byte_literal(message.discriminator) +
                    "}, std::uint16_t{" + std::to_string(message.size) +
                    "U}},");
  }
  append_line(output, 1U, "}};");
  append_line(output, 0U, "};");
  append_line(output, 0U);

  append_line(
      output, 0U,
      "static_assert(feedforge::runtime_api_version ==");
  append_line(output, 2U,
              "pipeline_metadata::required_runtime_api_version,");
  append_line(
      output, 2U,
      "\"FeedForge runtime API mismatch; regenerate this header with a "
      "compatible compiler\");");
  append_line(output, 0U,
              "static_assert(feedforge::decoder_implementation<");
  append_line(output, 2U,
              "feedforge::profile::portable_checked>,");
  append_line(output, 2U,
              "\"portable_checked must satisfy decoder_implementation\");");
  append_line(output, 0U,
              "static_assert(feedforge::profile::portable_checked::variant_id ==");
  append_line(output, 2U, "pipeline_metadata::profile_variant_id,");
  append_line(output, 2U,
              "\"generated profile variant does not match the runtime "
              "profile\");");
  append_line(output, 0U);

  append_line(output, 0U,
              "static_assert(pipeline_metadata::known_messages.size() == " +
                  std::to_string(ir.schema.messages.size()) +
                  "U, \"known message table size changed\");");
  for (std::size_t index = 0U; index < ir.schema.messages.size(); ++index) {
    const ffir_message_v1& message = ir.schema.messages[index];
    append_line(
        output, 0U,
        "static_assert(pipeline_metadata::known_messages[" +
            std::to_string(index) + "U].discriminator == std::byte{" +
            byte_literal(message.discriminator) +
            "}, \"known message discriminator changed\");");
    append_line(
        output, 0U,
        "static_assert(pipeline_metadata::known_messages[" +
            std::to_string(index) + "U].size == std::uint16_t{" +
            std::to_string(message.size) +
            "U}, \"known message size changed\");");
  }
  for (std::size_t left = 0U; left < ir.schema.messages.size(); ++left) {
    for (std::size_t right = left + 1U; right < ir.schema.messages.size();
         ++right) {
      append_line(
          output, 0U,
          "static_assert(pipeline_metadata::known_messages[" +
              std::to_string(left) +
              "U].discriminator != pipeline_metadata::known_messages[" +
              std::to_string(right) +
              "U].discriminator, \"known message discriminators must be "
              "unique\");");
    }
  }
  append_line(output, 0U);
}

void append_events(std::string& output, const ffir_v1& ir) {
  for (const ffir_event_v1& event : ir.pipeline.events) {
    append_line(output, 0U, "struct " + event.event + " {");
    for (const ffir_field_v1& field : event.fields) {
      append_line(output, 1U, *cpp_type(field) + " " + field.name + "{};");
    }
    append_line(output, 0U);
    append_line(output, 1U,
                "static constexpr std::byte source_discriminator{" +
                    byte_literal(event.source_discriminator) + "};");
    append_line(output, 1U,
                "static constexpr std::string_view event_name{\"" +
                    escape_cpp_string(event.event) + "\"};");
    append_line(output, 0U);
    append_line(output, 1U,
                "friend constexpr bool operator==(const " + event.event +
                    "&, const " + event.event + "&) = default;");
    append_line(output, 0U, "};");
    append_line(output, 0U);

    append_line(output, 0U,
                "static_assert(std::is_default_constructible_v<" +
                    event.event + ">);");
    append_line(output, 0U,
                "static_assert(std::is_aggregate_v<" + event.event + ">);");
    append_line(output, 0U,
                "static_assert(std::is_standard_layout_v<" + event.event +
                    ">);");
    append_line(output, 0U,
                "static_assert(std::is_trivially_copyable_v<" + event.event +
                    ">);");

    const std::size_t known_index =
        message_index(ir, event.source_discriminator);
    for (const ffir_field_v1& field : event.fields) {
      append_line(output, 0U,
                  "static_assert(std::same_as<decltype(" + event.event +
                      "::" + field.name + "), " + *cpp_type(field) +
                      ">, \"projected field type changed\");");
      append_line(
          output, 0U,
          "static_assert(" + std::to_string(field.offset) + "U + " +
              std::to_string(field.width) +
              "U <= pipeline_metadata::known_messages[" +
              std::to_string(known_index) +
              "U].size, \"projected field exceeds message bounds\");");
      if (field.physical == physical_kind::unsigned_integer) {
        append_line(
            output, 0U,
            "static_assert(feedforge::wire::supported_unsigned_width<" +
                std::to_string(field.width) +
                "U>, \"projected unsigned width is unsupported\");");
      } else {
        append_line(output, 0U,
                    "static_assert(" + std::to_string(field.width) +
                        "U > 0U, \"projected ASCII width must be positive\");");
      }
      if (field.logical == logical_kind::decimal) {
        append_line(
            output, 0U,
            "static_assert((" + std::to_string(field.width) +
                "U == 4U || " + std::to_string(field.width) +
                "U == 8U) && " + std::to_string(*field.scale) +
                "U <= 18U, \"projected decimal width or scale is "
                "unsupported\");");
      }
    }
    append_line(output, 0U);
  }

  for (std::size_t left = 0U; left < ir.pipeline.events.size(); ++left) {
    for (std::size_t right = left + 1U; right < ir.pipeline.events.size();
         ++right) {
      append_line(
          output, 0U,
          "static_assert(" + ir.pipeline.events[left].event +
              "::source_discriminator != " + ir.pipeline.events[right].event +
              "::source_discriminator, \"selected event discriminators must "
              "be unique\");");
      append_line(
          output, 0U,
          "static_assert(" + ir.pipeline.events[left].event +
              "::event_name != " + ir.pipeline.events[right].event +
              "::event_name, \"selected event names must be unique\");");
    }
  }
  if (ir.pipeline.events.size() > 1U) {
    append_line(output, 0U);
  }

  append_line(output, 0U, "template <class Sink>");
  append_line(output, 0U, "concept sink_for_all_selected_events =");
  for (std::size_t index = 0U; index < ir.pipeline.events.size(); ++index) {
    const std::string suffix =
        index + 1U == ir.pipeline.events.size() ? ";" : " &&";
    append_line(output, 2U,
                "feedforge::sink_for<Sink, " +
                    ir.pipeline.events[index].event + ">" + suffix);
  }
  append_line(output, 0U);
}

void append_decoder(std::string& output, const ffir_v1& ir) {
  append_line(output, 0U,
              "template <feedforge::decoder_implementation Implementation>");
  append_line(output, 0U, "class basic_decoder {");
  append_line(output, 0U, " public:");
  append_line(output, 1U, "template <class Sink>");
  append_line(output, 2U,
              "requires sink_for_all_selected_events<Sink>");
  append_line(output, 1U,
              "[[nodiscard]] FEEDFORGE_FORCE_INLINE "
              "feedforge::decode_outcome decode_one(");
  append_line(output, 3U,
              "std::span<const std::byte> payload, Sink& sink) const noexcept {");
  append_line(output, 2U,
              "const std::size_t actual_size = payload.size();");
  append_line(output, 2U, "if (payload.empty()) {");
  append_outcome(output, 3U, "empty_payload", "std::byte{0x00U}",
                 "std::uint16_t{0U}", "actual_size");
  append_line(output, 2U, "}");
  append_line(output, 0U);
  append_line(output, 2U, "const std::byte message_type = payload[0U];");
  append_line(
      output, 2U,
      "const std::uint8_t discriminator = "
      "std::to_integer<std::uint8_t>(message_type);");
  if (ir.pipeline.events.size() == ir.schema.messages.size()) {
    append_line(output, 2U, "switch (discriminator) {");
    for (const ffir_event_v1& event : ir.pipeline.events) {
      const ffir_message_v1* message =
          find_message(ir, event.source_discriminator);
      append_line(output, 3U,
                  "case " + byte_literal(event.source_discriminator) + ": {");
      append_line(output, 4U,
                  "constexpr std::uint16_t expected_size{" +
                      std::to_string(message->size) + "U};");
      append_line(
          output, 4U,
          "if (actual_size != static_cast<std::size_t>(expected_size)) {");
      append_outcome(output, 5U, "invalid_message_size", "message_type",
                     "expected_size", "actual_size");
      append_line(output, 4U, "}");
      append_line(output, 4U, event.event + " event{};");
      for (const ffir_field_v1& field : event.fields) {
        append_field_load(output, field);
      }
      append_line(output, 4U,
                  "if (sink(event) == feedforge::flow::stop) {");
      append_outcome(output, 5U, "stopped", "message_type", "expected_size",
                     "actual_size");
      append_line(output, 4U, "}");
      append_outcome(output, 4U, "emitted", "message_type", "expected_size",
                     "actual_size");
      append_line(output, 3U, "}");
    }
    append_line(output, 3U, "default:");
    append_outcome(
        output, 4U,
        ir.pipeline.unknown_messages == "skip" ? "unknown_skipped"
                                                : "unknown_message_type",
        "message_type", "std::uint16_t{0U}", "actual_size");
    append_line(output, 2U, "}");
  } else {
    append_line(output, 2U, "switch (discriminator) {");
    for (const ffir_event_v1& event : ir.pipeline.events) {
      const ffir_message_v1* message =
          find_message(ir, event.source_discriminator);
      append_line(output, 3U,
                  "case " + byte_literal(event.source_discriminator) + ": {");
      append_line(output, 4U,
                  "constexpr std::uint16_t expected_size{" +
                      std::to_string(message->size) + "U};");
      append_line(
          output, 4U,
          "if (actual_size != static_cast<std::size_t>(expected_size)) {");
      append_outcome(output, 5U, "invalid_message_size", "message_type",
                     "expected_size", "actual_size");
      append_line(output, 4U, "}");
      append_line(output, 4U, event.event + " event{};");
      for (const ffir_field_v1& field : event.fields) {
        append_field_load(output, field);
      }
      append_line(output, 4U,
                  "if (sink(event) == feedforge::flow::stop) {");
      append_outcome(output, 5U, "stopped", "message_type", "expected_size",
                     "actual_size");
      append_line(output, 4U, "}");
      append_outcome(output, 4U, "emitted", "message_type", "expected_size",
                     "actual_size");
      append_line(output, 3U, "}");
    }
    append_line(output, 3U, "default:");
    append_line(output, 4U, "break;");
    append_line(output, 2U, "}");
    append_line(output, 0U);

    append_line(output, 2U, "std::uint16_t expected_size{};");
    append_line(output, 2U, "switch (discriminator) {");
    for (const ffir_message_v1& message : ir.schema.messages) {
      const auto selected =
          std::ranges::find(ir.pipeline.events, message.discriminator,
                            &ffir_event_v1::source_discriminator);
      if (selected != ir.pipeline.events.end()) {
        continue;
      }
      append_line(output, 3U,
                  "case " + byte_literal(message.discriminator) + ":");
      append_line(output, 4U,
                  "expected_size = std::uint16_t{" +
                      std::to_string(message.size) + "U};");
      append_line(output, 4U, "break;");
    }
    append_line(output, 3U, "default:");
    append_outcome(
        output, 4U,
        ir.pipeline.unknown_messages == "skip" ? "unknown_skipped"
                                                : "unknown_message_type",
        "message_type", "std::uint16_t{0U}", "actual_size");
    append_line(output, 2U, "}");
    append_line(output, 0U);
    append_line(
        output, 2U,
        "if (actual_size != static_cast<std::size_t>(expected_size)) {");
    append_outcome(output, 3U, "invalid_message_size", "message_type",
                   "expected_size", "actual_size");
    append_line(output, 2U, "}");
    append_line(output, 0U);
    append_outcome(output, 2U, "known_unselected_skipped", "message_type",
                   "expected_size", "actual_size");
  }
  append_line(output, 1U, "}");
  append_line(output, 0U, "};");
  append_line(output, 0U);
  append_line(
      output, 0U,
      "using decoder = "
      "basic_decoder<feedforge::profile::portable_checked>;");
  append_line(output, 0U);
}

void append_replay(std::string& output, const ffir_v1& ir) {
  append_line(output, 0U, "template <class Sink>");
  append_line(output, 1U,
              "requires sink_for_all_selected_events<Sink>");
  append_line(output, 0U,
              "[[nodiscard]] feedforge::replay_summary replay_binary_file(");
  append_line(output, 2U,
              "std::span<const std::byte> input, Sink& sink) noexcept {");
  if (ir.pipeline.events.size() != ir.schema.messages.size()) {
    append_line(output, 1U, "decoder message_decoder{};");
    append_line(output, 1U, "feedforge::replay_summary summary{};");
    append_line(output, 1U, "std::size_t position{};");
    append_line(output, 0U);
    append_line(output, 1U, "for (;;) {");
    append_line(output, 2U,
                "const std::size_t available = input.size() - position;");
    append_line(output, 2U, "if (available == 0U) {");
    append_line(output, 3U,
                "summary.status = feedforge::replay_status::incomplete;");
    append_line(output, 3U, "summary.bytes_consumed = position;");
    append_line(output, 3U, "return summary;");
    append_line(output, 2U, "}");
    append_line(output, 2U, "if (available == 1U) {");
    append_line(output, 3U,
                "summary.status = feedforge::replay_status::framing_error;");
    append_line(output, 3U, "summary.bytes_consumed = position;");
    append_line(output, 3U, "summary.error_offset = position;");
    append_line(
        output, 3U,
        "summary.framing_error = "
        "feedforge::framing_errc::truncated_length_prefix;");
    append_line(output, 3U, "return summary;");
    append_line(output, 2U, "}");
    append_line(output, 0U);
    append_line(output, 2U, "const std::size_t frame_offset = position;");
    append_line(
        output, 2U,
        "const auto high = std::to_integer<std::uint16_t>(input[position]);");
    append_line(
        output, 2U,
        "const auto low = "
        "std::to_integer<std::uint16_t>(input[position + 1U]);");
    append_line(
        output, 2U,
        "const auto payload_size = "
        "static_cast<std::uint16_t>((high << 8U) | low);");
    append_line(output, 0U);
    append_line(output, 2U, "if (payload_size == 0U) {");
    append_line(output, 3U, "position += 2U;");
    append_line(output, 3U, "if (position != input.size()) {");
    append_line(output, 4U,
                "summary.status = feedforge::replay_status::framing_error;");
    append_line(output, 4U, "summary.bytes_consumed = position;");
    append_line(output, 4U, "summary.error_offset = position;");
    append_line(
        output, 4U,
        "summary.framing_error = "
        "feedforge::framing_errc::trailing_data_after_end_marker;");
    append_line(output, 4U, "return summary;");
    append_line(output, 3U, "}");
    append_line(output, 3U,
                "summary.status = feedforge::replay_status::complete;");
    append_line(output, 3U, "summary.bytes_consumed = position;");
    append_line(output, 3U, "return summary;");
    append_line(output, 2U, "}");
    append_line(output, 0U);
    append_line(
        output, 2U,
        "if (static_cast<std::size_t>(payload_size) > available - 2U) {");
    append_line(output, 3U,
                "summary.status = feedforge::replay_status::framing_error;");
    append_line(output, 3U, "summary.bytes_consumed = position;");
    append_line(output, 3U, "summary.error_offset = frame_offset;");
    append_line(
        output, 3U,
        "summary.framing_error = feedforge::framing_errc::truncated_payload;");
    append_line(output, 3U, "return summary;");
    append_line(output, 2U, "}");
    append_line(output, 0U);
    append_line(
        output, 2U,
        "const auto payload = input.subspan(position + 2U, payload_size);");
    append_line(
        output, 2U,
        "position += 2U + static_cast<std::size_t>(payload_size);");
    append_line(output, 2U, "++summary.frames_seen;");
    append_line(
        output, 2U,
        "const feedforge::decode_outcome outcome = "
        "message_decoder.decode_one(payload, sink);");
    append_line(output, 2U,
                "if (outcome.status == feedforge::decode_status::emitted) {");
    append_line(output, 3U, "++summary.events_emitted;");
    append_line(output, 3U, "continue;");
    append_line(output, 2U, "}");
    append_line(
        output, 2U,
        "if (outcome.status == "
        "feedforge::decode_status::known_unselected_skipped) {");
    append_line(output, 3U, "++summary.known_messages_skipped;");
    append_line(output, 3U, "continue;");
    append_line(output, 2U, "}");
    append_line(
        output, 2U,
        "if (outcome.status == feedforge::decode_status::unknown_skipped) {");
    append_line(output, 3U, "++summary.unknown_messages_skipped;");
    append_line(output, 3U, "continue;");
    append_line(output, 2U, "}");
    append_line(output, 2U,
                "if (outcome.status == feedforge::decode_status::stopped) {");
    append_line(output, 3U, "++summary.events_emitted;");
    append_line(output, 3U,
                "summary.status = feedforge::replay_status::stopped;");
    append_line(output, 3U, "summary.bytes_consumed = position;");
    append_line(output, 3U, "return summary;");
    append_line(output, 2U, "}");
    append_line(output, 2U,
                "summary.status = feedforge::replay_status::decode_error;");
    append_line(output, 2U, "summary.bytes_consumed = position;");
    append_line(output, 2U, "summary.error_offset = frame_offset + 2U;");
    append_line(output, 2U, "summary.decode_error = outcome;");
    append_line(output, 2U, "return summary;");
    append_line(output, 1U, "}");
    append_line(output, 0U, "}");
    append_line(output, 0U);
    return;
  }

  append_line(output, 1U, "feedforge::binary_file_cursor cursor{input};");
  append_line(output, 1U, "decoder message_decoder{};");
  append_line(output, 1U, "feedforge::replay_summary summary{};");
  append_line(output, 0U);
  append_line(output, 1U, "for (;;) {");
  append_line(output, 2U,
              "const feedforge::frame_outcome frame = cursor.next();");
  append_line(output, 2U, "switch (frame.status) {");
  append_line(output, 3U, "case feedforge::frame_status::frame: {");
  append_line(output, 4U, "++summary.frames_seen;");
  append_line(
      output, 4U,
      "const feedforge::decode_outcome outcome = "
      "message_decoder.decode_one(frame.frame.payload, sink);");
  append_line(output, 4U,
              "if (outcome.status == feedforge::decode_status::emitted) {");
  append_line(output, 5U, "++summary.events_emitted;");
  append_line(output, 5U, "continue;");
  append_line(output, 4U, "}");
  append_line(
      output, 4U,
      "if (outcome.status == "
      "feedforge::decode_status::known_unselected_skipped) {");
  append_line(output, 5U, "++summary.known_messages_skipped;");
  append_line(output, 5U, "continue;");
  append_line(output, 4U, "}");
  append_line(
      output, 4U,
      "if (outcome.status == feedforge::decode_status::unknown_skipped) {");
  append_line(output, 5U, "++summary.unknown_messages_skipped;");
  append_line(output, 5U, "continue;");
  append_line(output, 4U, "}");
  append_line(output, 4U,
              "if (outcome.status == feedforge::decode_status::stopped) {");
  append_line(output, 5U, "++summary.events_emitted;");
  append_line(output, 5U,
              "summary.status = feedforge::replay_status::stopped;");
  append_line(output, 5U,
              "summary.bytes_consumed = cursor.consumed();");
  append_line(output, 5U, "return summary;");
  append_line(output, 4U, "}");
  append_line(output, 4U,
              "summary.status = feedforge::replay_status::decode_error;");
  append_line(output, 4U,
              "summary.bytes_consumed = cursor.consumed();");
  append_line(
      output, 4U,
      "summary.error_offset = "
      "static_cast<std::size_t>(frame.frame.file_offset) + 2U;");
  append_line(output, 4U, "summary.decode_error = outcome;");
  append_line(output, 4U, "return summary;");
  append_line(output, 3U, "}");
  append_line(output, 3U, "case feedforge::frame_status::complete:");
  append_line(output, 4U, "if (!cursor.remaining().empty()) {");
  append_line(output, 5U,
              "summary.status = feedforge::replay_status::framing_error;");
  append_line(output, 5U,
              "summary.bytes_consumed = cursor.consumed();");
  append_line(output, 5U,
              "summary.error_offset = cursor.consumed();");
  append_line(
      output, 5U,
      "summary.framing_error = "
      "feedforge::framing_errc::trailing_data_after_end_marker;");
  append_line(output, 5U, "return summary;");
  append_line(output, 4U, "}");
  append_line(output, 4U,
              "summary.status = feedforge::replay_status::complete;");
  append_line(output, 4U,
              "summary.bytes_consumed = cursor.consumed();");
  append_line(output, 4U, "return summary;");
  append_line(output, 3U, "case feedforge::frame_status::incomplete:");
  append_line(output, 4U,
              "summary.status = feedforge::replay_status::incomplete;");
  append_line(output, 4U,
              "summary.bytes_consumed = cursor.consumed();");
  append_line(output, 4U, "return summary;");
  append_line(output, 3U, "case feedforge::frame_status::error:");
  append_line(output, 4U,
              "summary.status = feedforge::replay_status::framing_error;");
  append_line(output, 4U,
              "summary.bytes_consumed = cursor.consumed();");
  append_line(output, 4U,
              "summary.error_offset = "
              "static_cast<std::size_t>(frame.offset);");
  append_line(output, 4U, "summary.framing_error = frame.error;");
  append_line(output, 4U, "return summary;");
  append_line(output, 2U, "}");
  append_line(output, 1U, "}");
  append_line(output, 0U, "}");
  append_line(output, 0U);
}

void append_footer(std::string& output, const ffir_v1& ir) {
  append_line(output, 0U,
              "}  // namespace " + ir.pipeline.cpp_namespace);
  append_line(output, 0U);
  append_line(output, 0U, "#endif  // " + include_guard(ir));
}

}  // namespace

result<std::string> emit_cpp(const ffir_v1& ir) {
  if (auto valid = validate_ir(ir); !valid) {
    return std::unexpected(std::move(valid.error()));
  }

  std::string output;
  output.reserve(8192U + ir.pipeline.events.size() * 2048U);
  append_header(output, ir);
  append_metadata(output, ir);
  append_events(output, ir);
  append_decoder(output, ir);
  append_replay(output, ir);
  append_footer(output, ir);
  return output;
}

}  // namespace feedforge::compiler
