#include "parse_toml.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <utility>

#include <toml++/toml.hpp>

namespace feedforge::compiler {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] source_mark mark_from(const toml::source_region& region) {
  return source_mark{source_position{
      static_cast<std::size_t>(region.begin.line),
      static_cast<std::size_t>(region.begin.column),
  }};
}

[[nodiscard]] source_mark mark_from(const toml::node& node) {
  return mark_from(node.source());
}

[[nodiscard]] diagnostic structural_problem(
    const std::string_view source_path, const source_mark& mark,
    std::string code, std::string object_path, std::string message) {
  return make_diagnostic(std::move(code), std::string(source_path), mark,
                         std::move(object_path), std::move(message));
}

[[nodiscard]] result<toml::table> parse_document(
    const std::string_view text, const std::string_view source_path,
    const std::string_view object_path) {
#if TOML_EXCEPTIONS
  try {
    return toml::parse(text, std::string(source_path));
  } catch (const toml::parse_error& error) {
    return std::unexpected(structural_problem(
        source_path, mark_from(error.source()), "FFTOML001",
        std::string(object_path), std::string(error.description())));
  }
#else
  toml::parse_result parsed = toml::parse(text, std::string(source_path));
  if (!parsed) {
    const toml::parse_error& error = parsed.error();
    return std::unexpected(structural_problem(
        source_path, mark_from(error.source()), "FFTOML001",
        std::string(object_path), std::string(error.description())));
  }
  return std::move(parsed).table();
#endif
}

template <std::size_t Size>
[[nodiscard]] result<void> reject_unknown_keys(
    const toml::table& table,
    const std::array<std::string_view, Size>& accepted,
    const std::string_view source_path, const std::string_view object_path,
    const std::string_view code) {
  for (const auto& [key, value] : table) {
    if (std::ranges::find(accepted, key.str()) == accepted.end()) {
      return std::unexpected(structural_problem(
          source_path, mark_from(value), std::string(code),
          std::string(object_path) + "." + std::string(key.str()),
          "unknown key '" + std::string(key.str()) + "'"));
    }
  }
  return {};
}

[[nodiscard]] result<std::string> required_string(
    const toml::table& table, const std::string_view key,
    const std::string_view source_path, const std::string_view object_path,
    const std::string_view missing_code, const std::string_view type_code) {
  const toml::node* node = table.get(key);
  if (node == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(table), std::string(missing_code),
        std::string(object_path) + "." + std::string(key),
        "missing required key '" + std::string(key) + "'"));
  }
  auto value = node->value<std::string>();
  if (!value) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*node), std::string(type_code),
        std::string(object_path) + "." + std::string(key),
        "key '" + std::string(key) + "' must be a string"));
  }
  return std::move(*value);
}

[[nodiscard]] result<std::int64_t> required_integer(
    const toml::table& table, const std::string_view key,
    const std::string_view source_path, const std::string_view object_path,
    const std::string_view missing_code, const std::string_view type_code) {
  const toml::node* node = table.get(key);
  if (node == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(table), std::string(missing_code),
        std::string(object_path) + "." + std::string(key),
        "missing required key '" + std::string(key) + "'"));
  }
  auto value = node->value<std::int64_t>();
  if (!value) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*node), std::string(type_code),
        std::string(object_path) + "." + std::string(key),
        "key '" + std::string(key) + "' must be an integer"));
  }
  return *value;
}

[[nodiscard]] result<std::optional<std::string>> optional_string(
    const toml::table& table, const std::string_view key,
    const std::string_view source_path, const std::string_view object_path,
    const std::string_view type_code) {
  const toml::node* node = table.get(key);
  if (node == nullptr) {
    return std::optional<std::string>{};
  }
  auto value = node->value<std::string>();
  if (!value) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*node), std::string(type_code),
        std::string(object_path) + "." + std::string(key),
        "key '" + std::string(key) + "' must be a string"));
  }
  return std::optional<std::string>{std::move(*value)};
}

[[nodiscard]] result<std::optional<std::int64_t>> optional_integer(
    const toml::table& table, const std::string_view key,
    const std::string_view source_path, const std::string_view object_path,
    const std::string_view type_code) {
  const toml::node* node = table.get(key);
  if (node == nullptr) {
    return std::optional<std::int64_t>{};
  }
  auto value = node->value<std::int64_t>();
  if (!value) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*node), std::string(type_code),
        std::string(object_path) + "." + std::string(key),
        "key '" + std::string(key) + "' must be an integer"));
  }
  return std::optional<std::int64_t>{*value};
}

[[nodiscard]] result<const toml::array*> required_array(
    const toml::table& table, const std::string_view key,
    const std::string_view source_path, const std::string_view object_path,
    const std::string_view missing_code, const std::string_view type_code) {
  const toml::node* node = table.get(key);
  if (node == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(table), std::string(missing_code),
        std::string(object_path) + "." + std::string(key),
        "missing required key '" + std::string(key) + "'"));
  }
  const toml::array* array = node->as_array();
  if (array == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*node), std::string(type_code),
        std::string(object_path) + "." + std::string(key),
        "key '" + std::string(key) + "' must be an array"));
  }
  return array;
}

[[nodiscard]] result<void> validate_optional_documentation(
    const toml::table& table, const std::string_view source_path,
    const std::string_view object_path, const std::string_view type_code,
    const bool include_spec_metadata) {
  auto description =
      optional_string(table, "description", source_path, object_path, type_code);
  if (!description) {
    return std::unexpected(std::move(description.error()));
  }
  if (!include_spec_metadata) {
    return {};
  }
  auto section =
      optional_string(table, "spec_section", source_path, object_path, type_code);
  if (!section) {
    return std::unexpected(std::move(section.error()));
  }
  auto page =
      optional_integer(table, "spec_page", source_path, object_path, type_code);
  if (!page) {
    return std::unexpected(std::move(page.error()));
  }
  if (*page && **page < 0) {
    const toml::node* node = table.get("spec_page");
    return std::unexpected(structural_problem(
        source_path, node == nullptr ? mark_from(table) : mark_from(*node),
        "FFSCHEMA032", std::string(object_path) + ".spec_page",
        "spec_page must be non-negative"));
  }
  return {};
}

[[nodiscard]] result<std::vector<std::string>> optional_string_array(
    const toml::table& table, const std::string_view key,
    const std::string_view source_path, const std::string_view object_path,
    const std::string_view type_code) {
  const toml::node* node = table.get(key);
  if (node == nullptr) {
    return std::vector<std::string>{};
  }
  const toml::array* array = node->as_array();
  if (array == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*node), std::string(type_code),
        std::string(object_path) + "." + std::string(key),
        "key '" + std::string(key) + "' must be an array of strings"));
  }
  std::vector<std::string> values;
  values.reserve(array->size());
  for (const toml::node& item : *array) {
    auto value = item.value<std::string>();
    if (!value) {
      return std::unexpected(structural_problem(
          source_path, mark_from(item), std::string(type_code),
          std::string(object_path) + "." + std::string(key),
          "key '" + std::string(key) + "' must contain only strings"));
    }
    values.push_back(std::move(*value));
  }
  return values;
}

[[nodiscard]] result<std::string> read_source_file(
    const std::string_view source_path, const std::string_view object_path) {
  const fs::path path{source_path};
  std::error_code error;
  if (!fs::is_regular_file(path, error) || error) {
    return std::unexpected(make_diagnostic(
        "FFIO001", std::string(source_path), source_mark{},
        std::string(object_path), "input is not a readable regular file"));
  }
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return std::unexpected(make_diagnostic(
        "FFIO001", std::string(source_path), source_mark{},
        std::string(object_path), "failed to open input file for reading"));
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  if (input.bad()) {
    return std::unexpected(make_diagnostic(
        "FFIO001", std::string(source_path), source_mark{},
        std::string(object_path), "failed while reading input file"));
  }
  return std::move(contents).str();
}

}  // namespace

result<schema_source> parse_schema_toml(const std::string_view text,
                                        const std::string_view source_path) {
  auto document = parse_document(text, source_path, "schema");
  if (!document) {
    return std::unexpected(std::move(document.error()));
  }
  constexpr std::array top_keys{
      std::string_view{"format_version"},
      std::string_view{"name"},
      std::string_view{"protocol_version"},
      std::string_view{"document_revision"},
      std::string_view{"wire_endian"},
      std::string_view{"discriminator_offset"},
      std::string_view{"discriminator_width"},
      std::string_view{"description"},
      std::string_view{"types"},
      std::string_view{"messages"},
  };
  if (auto checked = reject_unknown_keys(*document, top_keys, source_path,
                                         "schema", "FFSCHEMA002");
      !checked) {
    return std::unexpected(std::move(checked.error()));
  }
  if (auto checked = validate_optional_documentation(
          *document, source_path, "schema", "FFSCHEMA004", false);
      !checked) {
    return std::unexpected(std::move(checked.error()));
  }

  schema_source source{
      .mark = mark_from(*document),
      .source_path = normalise_source_path(source_path),
      .name = {},
      .protocol_version = {},
      .document_revision = {},
      .wire_endian = {},
      .types = {},
      .messages = {},
  };
  auto format_version =
      required_integer(*document, "format_version", source_path, "schema",
                       "FFSCHEMA001", "FFSCHEMA004");
  auto name = required_string(*document, "name", source_path, "schema",
                              "FFSCHEMA003", "FFSCHEMA004");
  auto protocol_version =
      required_string(*document, "protocol_version", source_path, "schema",
                      "FFSCHEMA003", "FFSCHEMA004");
  auto document_revision =
      required_string(*document, "document_revision", source_path, "schema",
                      "FFSCHEMA003", "FFSCHEMA004");
  auto wire_endian =
      required_string(*document, "wire_endian", source_path, "schema",
                      "FFSCHEMA003", "FFSCHEMA004");
  auto discriminator_offset =
      required_integer(*document, "discriminator_offset", source_path, "schema",
                       "FFSCHEMA003", "FFSCHEMA004");
  auto discriminator_width =
      required_integer(*document, "discriminator_width", source_path, "schema",
                       "FFSCHEMA003", "FFSCHEMA004");
  if (!format_version) {
    return std::unexpected(std::move(format_version.error()));
  }
  if (!name) {
    return std::unexpected(std::move(name.error()));
  }
  if (!protocol_version) {
    return std::unexpected(std::move(protocol_version.error()));
  }
  if (!document_revision) {
    return std::unexpected(std::move(document_revision.error()));
  }
  if (!wire_endian) {
    return std::unexpected(std::move(wire_endian.error()));
  }
  if (!discriminator_offset) {
    return std::unexpected(std::move(discriminator_offset.error()));
  }
  if (!discriminator_width) {
    return std::unexpected(std::move(discriminator_width.error()));
  }
  source.format_version = *format_version;
  source.name = std::move(*name);
  source.protocol_version = std::move(*protocol_version);
  source.document_revision = std::move(*document_revision);
  source.wire_endian = std::move(*wire_endian);
  source.discriminator_offset = *discriminator_offset;
  source.discriminator_width = *discriminator_width;

  if (const toml::node* types_node = document->get("types");
      types_node != nullptr) {
    const toml::array* types = types_node->as_array();
    if (types == nullptr) {
      return std::unexpected(structural_problem(
          source_path, mark_from(*types_node), "FFSCHEMA004", "schema.types",
          "types must be an array of tables"));
    }
    constexpr std::array type_keys{
        std::string_view{"name"},        std::string_view{"kind"},
        std::string_view{"width"},       std::string_view{"logical"},
        std::string_view{"scale"},       std::string_view{"description"},
    };
    source.types.reserve(types->size());
    for (std::size_t index = 0; index < types->size(); ++index) {
      const toml::node& item = (*types)[index];
      const toml::table* table = item.as_table();
      const std::string object_path =
          "schema.types[" + std::to_string(index) + ']';
      if (table == nullptr) {
        return std::unexpected(structural_problem(
            source_path, mark_from(item), "FFSCHEMA004", object_path,
            "each types entry must be a table"));
      }
      if (auto checked = reject_unknown_keys(
              *table, type_keys, source_path, object_path, "FFSCHEMA002");
          !checked) {
        return std::unexpected(std::move(checked.error()));
      }
      if (auto checked = validate_optional_documentation(
              *table, source_path, object_path, "FFSCHEMA004", false);
          !checked) {
        return std::unexpected(std::move(checked.error()));
      }
      auto type_name =
          required_string(*table, "name", source_path, object_path,
                          "FFSCHEMA003", "FFSCHEMA004");
      auto kind = required_string(*table, "kind", source_path, object_path,
                                  "FFSCHEMA003", "FFSCHEMA004");
      auto width = required_integer(*table, "width", source_path, object_path,
                                    "FFSCHEMA003", "FFSCHEMA004");
      auto logical = optional_string(*table, "logical", source_path,
                                     object_path, "FFSCHEMA004");
      auto scale = optional_integer(*table, "scale", source_path, object_path,
                                    "FFSCHEMA004");
      if (!type_name) {
        return std::unexpected(std::move(type_name.error()));
      }
      if (!kind) {
        return std::unexpected(std::move(kind.error()));
      }
      if (!width) {
        return std::unexpected(std::move(width.error()));
      }
      if (!logical) {
        return std::unexpected(std::move(logical.error()));
      }
      if (!scale) {
        return std::unexpected(std::move(scale.error()));
      }
      source.types.push_back(type_source{
          .mark = mark_from(*table),
          .name = std::move(*type_name),
          .kind = std::move(*kind),
          .width = *width,
          .logical = std::move(*logical),
          .scale = *scale,
      });
    }
  }

  const toml::node* messages_node = document->get("messages");
  if (messages_node == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*document), "FFSCHEMA003", "schema.messages",
        "missing required key 'messages'"));
  }
  const toml::array* messages = messages_node->as_array();
  if (messages == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*messages_node), "FFSCHEMA004",
        "schema.messages", "messages must be an array of tables"));
  }
  constexpr std::array message_keys{
      std::string_view{"name"},         std::string_view{"type"},
      std::string_view{"size"},         std::string_view{"description"},
      std::string_view{"spec_section"}, std::string_view{"spec_page"},
      std::string_view{"fields"},
  };
  constexpr std::array field_keys{
      std::string_view{"name"},         std::string_view{"type"},
      std::string_view{"offset"},       std::string_view{"width"},
      std::string_view{"role"},         std::string_view{"value"},
      std::string_view{"allowed"},      std::string_view{"description"},
      std::string_view{"spec_section"}, std::string_view{"spec_page"},
  };
  source.messages.reserve(messages->size());
  for (std::size_t message_index = 0; message_index < messages->size();
       ++message_index) {
    const toml::node& item = (*messages)[message_index];
    const toml::table* table = item.as_table();
    std::string object_path =
        "schema.messages[" + std::to_string(message_index) + ']';
    if (table == nullptr) {
      return std::unexpected(structural_problem(
          source_path, mark_from(item), "FFSCHEMA004", object_path,
          "each messages entry must be a table"));
    }
    if (auto checked = reject_unknown_keys(
            *table, message_keys, source_path, object_path, "FFSCHEMA002");
        !checked) {
      return std::unexpected(std::move(checked.error()));
    }
    if (auto checked = validate_optional_documentation(
            *table, source_path, object_path, "FFSCHEMA004", true);
        !checked) {
      return std::unexpected(std::move(checked.error()));
    }
    auto message_name =
        required_string(*table, "name", source_path, object_path,
                        "FFSCHEMA003", "FFSCHEMA004");
    auto message_type =
        required_string(*table, "type", source_path, object_path,
                        "FFSCHEMA003", "FFSCHEMA004");
    auto message_size =
        required_integer(*table, "size", source_path, object_path,
                         "FFSCHEMA003", "FFSCHEMA004");
    if (!message_name) {
      return std::unexpected(std::move(message_name.error()));
    }
    if (!message_type) {
      return std::unexpected(std::move(message_type.error()));
    }
    if (!message_size) {
      return std::unexpected(std::move(message_size.error()));
    }
    if (!message_name->empty()) {
      object_path = "schema.messages." + *message_name;
    }
    auto fields = required_array(*table, "fields", source_path, object_path,
                                 "FFSCHEMA003", "FFSCHEMA004");
    if (!fields) {
      return std::unexpected(std::move(fields.error()));
    }

    message_source parsed_message{
        .mark = mark_from(*table),
        .name = std::move(*message_name),
        .type = std::move(*message_type),
        .size = *message_size,
        .fields = {},
    };
    parsed_message.fields.reserve((*fields)->size());
    for (std::size_t field_index = 0; field_index < (*fields)->size();
         ++field_index) {
      const toml::node& field_item = (**fields)[field_index];
      const toml::table* field_table = field_item.as_table();
      std::string field_path =
          object_path + ".fields[" + std::to_string(field_index) + ']';
      if (field_table == nullptr) {
        return std::unexpected(structural_problem(
            source_path, mark_from(field_item), "FFSCHEMA004", field_path,
            "each fields entry must be a table"));
      }
      if (auto checked = reject_unknown_keys(
              *field_table, field_keys, source_path, field_path, "FFSCHEMA002");
          !checked) {
        return std::unexpected(std::move(checked.error()));
      }
      if (auto checked = validate_optional_documentation(
              *field_table, source_path, field_path, "FFSCHEMA004", true);
          !checked) {
        return std::unexpected(std::move(checked.error()));
      }
      auto field_name =
          required_string(*field_table, "name", source_path, field_path,
                          "FFSCHEMA003", "FFSCHEMA004");
      auto field_type =
          required_string(*field_table, "type", source_path, field_path,
                          "FFSCHEMA003", "FFSCHEMA004");
      auto offset =
          required_integer(*field_table, "offset", source_path, field_path,
                           "FFSCHEMA003", "FFSCHEMA004");
      auto width =
          required_integer(*field_table, "width", source_path, field_path,
                           "FFSCHEMA003", "FFSCHEMA004");
      auto role = optional_string(*field_table, "role", source_path, field_path,
                                  "FFSCHEMA004");
      auto value =
          optional_string(*field_table, "value", source_path, field_path,
                          "FFSCHEMA004");
      auto allowed = optional_string_array(*field_table, "allowed", source_path,
                                           field_path, "FFSCHEMA004");
      if (!field_name) {
        return std::unexpected(std::move(field_name.error()));
      }
      if (!field_type) {
        return std::unexpected(std::move(field_type.error()));
      }
      if (!offset) {
        return std::unexpected(std::move(offset.error()));
      }
      if (!width) {
        return std::unexpected(std::move(width.error()));
      }
      if (!role) {
        return std::unexpected(std::move(role.error()));
      }
      if (!value) {
        return std::unexpected(std::move(value.error()));
      }
      if (!allowed) {
        return std::unexpected(std::move(allowed.error()));
      }
      parsed_message.fields.push_back(field_source{
          .mark = mark_from(*field_table),
          .name = std::move(*field_name),
          .type = std::move(*field_type),
          .offset = *offset,
          .width = *width,
          .role = std::move(*role),
          .value = std::move(*value),
          .allowed = std::move(*allowed),
      });
    }
    source.messages.push_back(std::move(parsed_message));
  }
  return source;
}

result<pipeline_source> parse_pipeline_toml(
    const std::string_view text, const std::string_view source_path) {
  auto document = parse_document(text, source_path, "pipeline");
  if (!document) {
    return std::unexpected(std::move(document.error()));
  }
  constexpr std::array top_keys{
      std::string_view{"format_version"},
      std::string_view{"name"},
      std::string_view{"namespace"},
      std::string_view{"schema"},
      std::string_view{"profile"},
      std::string_view{"unknown_messages"},
      std::string_view{"unselected_messages"},
      std::string_view{"emit"},
  };
  if (auto checked = reject_unknown_keys(*document, top_keys, source_path,
                                         "pipeline", "FFPIPE002");
      !checked) {
    return std::unexpected(std::move(checked.error()));
  }

  pipeline_source source{
      .mark = mark_from(*document),
      .source_path = normalise_source_path(source_path),
      .name = {},
      .cpp_namespace = {},
      .schema = {},
      .profile = {},
      .unknown_messages = {},
      .unselected_messages = {},
      .projections = {},
  };
  auto format_version =
      required_integer(*document, "format_version", source_path, "pipeline",
                       "FFPIPE001", "FFPIPE004");
  auto name = required_string(*document, "name", source_path, "pipeline",
                              "FFPIPE003", "FFPIPE004");
  auto cpp_namespace =
      required_string(*document, "namespace", source_path, "pipeline",
                      "FFPIPE003", "FFPIPE004");
  auto schema = required_string(*document, "schema", source_path, "pipeline",
                                "FFPIPE003", "FFPIPE004");
  auto profile = required_string(*document, "profile", source_path, "pipeline",
                                 "FFPIPE003", "FFPIPE004");
  auto unknown_messages =
      required_string(*document, "unknown_messages", source_path, "pipeline",
                      "FFPIPE003", "FFPIPE004");
  auto unselected_messages =
      required_string(*document, "unselected_messages", source_path, "pipeline",
                      "FFPIPE003", "FFPIPE004");
  if (!format_version) {
    return std::unexpected(std::move(format_version.error()));
  }
  if (!name) {
    return std::unexpected(std::move(name.error()));
  }
  if (!cpp_namespace) {
    return std::unexpected(std::move(cpp_namespace.error()));
  }
  if (!schema) {
    return std::unexpected(std::move(schema.error()));
  }
  if (!profile) {
    return std::unexpected(std::move(profile.error()));
  }
  if (!unknown_messages) {
    return std::unexpected(std::move(unknown_messages.error()));
  }
  if (!unselected_messages) {
    return std::unexpected(std::move(unselected_messages.error()));
  }
  source.format_version = *format_version;
  source.name = std::move(*name);
  source.cpp_namespace = std::move(*cpp_namespace);
  source.schema = std::move(*schema);
  source.profile = std::move(*profile);
  source.unknown_messages = std::move(*unknown_messages);
  source.unselected_messages = std::move(*unselected_messages);

  const toml::node* emit_node = document->get("emit");
  if (emit_node == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*document), "FFPIPE003", "pipeline.emit",
        "missing required key 'emit'"));
  }
  const toml::array* emits = emit_node->as_array();
  if (emits == nullptr) {
    return std::unexpected(structural_problem(
        source_path, mark_from(*emit_node), "FFPIPE004", "pipeline.emit",
        "emit must be an array of tables"));
  }
  constexpr std::array emit_keys{
      std::string_view{"source"},
      std::string_view{"event"},
      std::string_view{"fields"},
  };
  source.projections.reserve(emits->size());
  for (std::size_t index = 0; index < emits->size(); ++index) {
    const toml::node& item = (*emits)[index];
    const toml::table* table = item.as_table();
    std::string object_path = "pipeline.emit[" + std::to_string(index) + ']';
    if (table == nullptr) {
      return std::unexpected(structural_problem(
          source_path, mark_from(item), "FFPIPE004", object_path,
          "each emit entry must be a table"));
    }
    if (auto checked = reject_unknown_keys(
            *table, emit_keys, source_path, object_path, "FFPIPE002");
        !checked) {
      return std::unexpected(std::move(checked.error()));
    }
    auto selected_source =
        required_string(*table, "source", source_path, object_path,
                        "FFPIPE003", "FFPIPE004");
    auto event = required_string(*table, "event", source_path, object_path,
                                 "FFPIPE003", "FFPIPE004");
    auto fields = required_array(*table, "fields", source_path, object_path,
                                 "FFPIPE003", "FFPIPE004");
    if (!selected_source) {
      return std::unexpected(std::move(selected_source.error()));
    }
    if (!event) {
      return std::unexpected(std::move(event.error()));
    }
    if (!fields) {
      return std::unexpected(std::move(fields.error()));
    }
    if (!event->empty()) {
      object_path = "pipeline.emit." + *event;
    }

    projection_source projection{
        .mark = mark_from(*table),
        .source = std::move(*selected_source),
        .event = std::move(*event),
        .fields = {},
        .field_marks = {},
    };
    projection.fields.reserve((*fields)->size());
    projection.field_marks.reserve((*fields)->size());
    for (const toml::node& field : **fields) {
      auto field_name = field.value<std::string>();
      if (!field_name) {
        return std::unexpected(structural_problem(
            source_path, mark_from(field), "FFPIPE004",
            object_path + ".fields", "fields must contain only strings"));
      }
      projection.fields.push_back(std::move(*field_name));
      projection.field_marks.push_back(mark_from(field));
    }
    source.projections.push_back(std::move(projection));
  }
  return source;
}

result<schema_source> parse_schema_file(const std::string_view source_path) {
  auto contents = read_source_file(source_path, "schema");
  if (!contents) {
    return std::unexpected(std::move(contents.error()));
  }
  return parse_schema_toml(*contents, source_path);
}

result<pipeline_source> parse_pipeline_file(const std::string_view source_path) {
  auto contents = read_source_file(source_path, "pipeline");
  if (!contents) {
    return std::unexpected(std::move(contents.error()));
  }
  return parse_pipeline_toml(*contents, source_path);
}

}  // namespace feedforge::compiler
