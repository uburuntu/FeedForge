#include <array>
#include <string>
#include <string_view>

#include "validation_test_support.hpp"

namespace {

namespace compiler = feedforge::compiler;
namespace support = feedforge::compiler::test;

void expect_parse_error(support::suite& tests, const std::string& text,
                        const std::string_view code,
                        const std::string_view object,
                        const std::string_view message,
                        const std::string_view description) {
  tests.expect_error(
      compiler::parse_schema_toml(text, "audit\\schema.toml"), code, object,
      message, description, std::string_view{"audit/schema.toml"});
}

void test_closed_grammar(support::suite& tests) {
  const std::string base{support::valid_schema_toml};

  expect_parse_error(
      tests,
      support::replace_once(tests, base, "[[types]]",
                            "unknown_top = true\n\n[[types]]",
                            "top-level unknown key"),
      "FFSCHEMA002", "schema.unknown_top", "unknown key 'unknown_top'",
      "top-level unknown schema key");
  expect_parse_error(
      tests,
      support::replace_once(
          tests, base, "description = \"Payload alias\"\n",
          "description = \"Payload alias\"\nunknown_type = true\n",
          "type unknown key"),
      "FFSCHEMA002", "schema.types[0].unknown_type",
      "unknown key 'unknown_type'", "unknown types-table key");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "spec_page = 1\n\n[[messages.fields]]",
                            "spec_page = 1\nunknown_message = true\n\n"
                            "[[messages.fields]]",
                            "message unknown key"),
      "FFSCHEMA002", "schema.messages[0].unknown_message",
      "unknown key 'unknown_message'", "unknown messages-table key");
  expect_parse_error(
      tests,
      support::replace_once(
          tests, base,
          "spec_page = 1\n\n[[messages.fields]]\nname = \"payload\"",
          "spec_page = 1\nunknown_field = true\n\n"
          "[[messages.fields]]\nname = \"payload\"",
          "field unknown key"),
      "FFSCHEMA002", "schema.messages.event.fields[0].unknown_field",
      "unknown key 'unknown_field'", "unknown messages.fields-table key");
}

void test_required_top_level_keys(support::suite& tests) {
  const std::string base{support::valid_schema_toml};
  struct required_case {
    std::string_view line;
    std::string_view code;
    std::string_view object;
    std::string_view key;
  };
  constexpr std::array cases{
      required_case{"format_version = 1\n", "FFSCHEMA001",
                    "schema.format_version", "format_version"},
      required_case{"name = \"test_feed\"\n", "FFSCHEMA003", "schema.name",
                    "name"},
      required_case{"protocol_version = \"1.0\"\n", "FFSCHEMA003",
                    "schema.protocol_version", "protocol_version"},
      required_case{"document_revision = \"2026-07-14\"\n", "FFSCHEMA003",
                    "schema.document_revision", "document_revision"},
      required_case{"wire_endian = \"big\"\n", "FFSCHEMA003",
                    "schema.wire_endian", "wire_endian"},
      required_case{"discriminator_offset = 0\n", "FFSCHEMA003",
                    "schema.discriminator_offset", "discriminator_offset"},
      required_case{"discriminator_width = 1\n", "FFSCHEMA003",
                    "schema.discriminator_width", "discriminator_width"},
  };
  for (const required_case& item : cases) {
    const std::string description =
        "missing schema key " + std::string(item.key);
    expect_parse_error(
        tests, support::erase_once(tests, base, item.line, description),
        item.code, item.object,
        "missing required key '" + std::string(item.key) + "'", description);
  }

  std::string without_messages = base;
  const std::size_t messages = without_messages.find("[[messages]]");
  tests.check(messages != std::string::npos,
              "messages mutation source exists");
  if (messages != std::string::npos) {
    without_messages.erase(messages);
  }
  expect_parse_error(tests, without_messages, "FFSCHEMA003",
                     "schema.messages", "missing required key 'messages'",
                     "missing messages array");
}

void test_required_nested_keys(support::suite& tests) {
  const std::string base{support::valid_schema_toml};

  struct nested_case {
    std::string_view text;
    std::string_view replacement;
    std::string_view object;
    std::string_view key;
    std::string_view description;
  };
  constexpr std::array type_cases{
      nested_case{"name = \"payload_type\"\n", "",
                  "schema.types[0].name", "name", "type missing name"},
      nested_case{"kind = \"ascii\"\n", "", "schema.types[0].kind", "kind",
                  "type missing kind"},
      nested_case{"width = 1\nlogical = \"ascii\"",
                  "logical = \"ascii\"", "schema.types[0].width", "width",
                  "type missing width"},
  };
  for (const nested_case& item : type_cases) {
    expect_parse_error(
        tests,
        support::replace_once(tests, base, item.text, item.replacement,
                              item.description),
        "FFSCHEMA003", item.object,
        "missing required key '" + std::string(item.key) + "'",
        item.description);
  }

  constexpr std::array message_cases{
      nested_case{"name = \"event\"\n", "", "schema.messages[0].name",
                  "name", "message missing name"},
      nested_case{"type = \"A\"\n", "", "schema.messages[0].type", "type",
                  "message missing type"},
      nested_case{"size = 2\n", "", "schema.messages[0].size", "size",
                  "message missing size"},
  };
  for (const nested_case& item : message_cases) {
    expect_parse_error(
        tests,
        support::replace_once(tests, base, item.text, item.replacement,
                              item.description),
        "FFSCHEMA003", item.object,
        "missing required key '" + std::string(item.key) + "'",
        item.description);
  }

  std::string without_fields = base;
  const std::size_t fields = without_fields.find("[[messages.fields]]");
  tests.check(fields != std::string::npos, "fields mutation source exists");
  if (fields != std::string::npos) {
    without_fields.erase(fields);
  }
  expect_parse_error(tests, without_fields, "FFSCHEMA003",
                     "schema.messages.event.fields",
                     "missing required key 'fields'",
                     "message missing nested fields");

  constexpr std::array field_cases{
      nested_case{"name = \"message_type\"\n", "",
                  "schema.messages.event.fields[0].name", "name",
                  "field missing name"},
      nested_case{"type = \"alpha\"\noffset = 0",
                  "offset = 0", "schema.messages.event.fields[0].type", "type",
                  "field missing type"},
      nested_case{"offset = 0\nwidth = 1", "width = 1",
                  "schema.messages.event.fields[0].offset", "offset",
                  "field missing offset"},
      nested_case{"width = 1\nrole = \"discriminator\"",
                  "role = \"discriminator\"",
                  "schema.messages.event.fields[0].width", "width",
                  "field missing width"},
  };
  for (const nested_case& item : field_cases) {
    expect_parse_error(
        tests,
        support::replace_once(tests, base, item.text, item.replacement,
                              item.description),
        "FFSCHEMA003", item.object,
        "missing required key '" + std::string(item.key) + "'",
        item.description);
  }
}

void test_toml_types_and_duplicates(support::suite& tests) {
  const std::string base{support::valid_schema_toml};

  expect_parse_error(
      tests,
      support::replace_once(tests, base, "format_version = 1",
                            "format_version = \"one\"",
                            "schema version TOML type"),
      "FFSCHEMA004", "schema.format_version", "must be an integer",
      "schema integer key with string value");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "width = 1\nlogical = \"ascii\"",
                            "width = \"one\"\nlogical = \"ascii\"",
                            "type width TOML type"),
      "FFSCHEMA004", "schema.types[0].width", "must be an integer",
      "type integer key with string value");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "size = 2", "size = \"two\"",
                            "message size TOML type"),
      "FFSCHEMA004", "schema.messages[0].size", "must be an integer",
      "message integer key with string value");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "type = \"alpha\"\noffset = 0",
                            "type = \"alpha\"\noffset = \"zero\"",
                            "field offset TOML type"),
      "FFSCHEMA004", "schema.messages.event.fields[0].offset",
      "must be an integer", "field integer key with string value");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "allowed = [\"X\"]",
                            "allowed = \"X\"", "allowed array TOML type"),
      "FFSCHEMA004", "schema.messages.event.fields[1].allowed",
      "must be an array of strings", "allowed metadata must be an array");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "allowed = [\"X\"]",
                            "allowed = [\"X\", 1]",
                            "allowed element TOML type"),
      "FFSCHEMA004", "schema.messages.event.fields[1].allowed",
      "must contain only strings",
      "allowed metadata rejects non-string element");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "spec_page = 1\n\n[[messages.fields]]",
                            "spec_page = -1\n\n[[messages.fields]]",
                            "negative message spec page"),
      "FFSCHEMA032", "schema.messages[0].spec_page",
      "spec_page must be non-negative", "negative message metadata integer");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "name = \"test_feed\"",
                            "name = \"test_feed\"\nname = \"duplicate\"",
                            "duplicate schema name key"),
      "FFTOML001", "schema", "name", "duplicate schema TOML name key");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "size = 2",
                            "size = 9223372036854775808",
                            "out-of-range TOML integer"),
      "FFTOML001", "schema", "integer",
      "TOML integer outside signed 64-bit range");
}

void test_table_shapes_and_optional_value_types(support::suite& tests) {
  const std::string base{support::valid_schema_toml};
  const std::size_t type_start = base.find("[[types]]");
  const std::size_t message_start = base.find("[[messages]]");
  tests.check(type_start != std::string::npos &&
                  message_start != std::string::npos &&
                  type_start < message_start,
              "schema table-shape type range exists");

  if (type_start != std::string::npos &&
      message_start != std::string::npos && type_start < message_start) {
    std::string scalar_types = base;
    scalar_types.replace(type_start, message_start - type_start,
                         "types = \"wrong\"\n\n");
    expect_parse_error(tests, scalar_types, "FFSCHEMA004", "schema.types",
                       "types must be an array of tables",
                       "types value is not an array");

    std::string non_table_type = base;
    non_table_type.replace(type_start, message_start - type_start,
                           "types = [\"wrong\"]\n\n");
    expect_parse_error(tests, non_table_type, "FFSCHEMA004",
                       "schema.types[0]",
                       "each types entry must be a table",
                       "types array entry is not a table");
  }

  if (type_start != std::string::npos) {
    std::string scalar_messages = base.substr(0, type_start);
    scalar_messages += "messages = \"wrong\"\n";
    expect_parse_error(tests, scalar_messages, "FFSCHEMA004",
                       "schema.messages",
                       "messages must be an array of tables",
                       "messages value is not an array");

    std::string non_table_message = base.substr(0, type_start);
    non_table_message += "messages = [\"wrong\"]\n";
    expect_parse_error(tests, non_table_message, "FFSCHEMA004",
                       "schema.messages[0]",
                       "each messages entry must be a table",
                       "messages array entry is not a table");
  }

  const std::size_t field_start = base.find("[[messages.fields]]");
  tests.check(field_start != std::string::npos,
              "schema table-shape field range exists");
  if (field_start != std::string::npos) {
    std::string scalar_fields = base.substr(0, field_start);
    scalar_fields += "fields = \"wrong\"\n";
    expect_parse_error(tests, scalar_fields, "FFSCHEMA004",
                       "schema.messages.event.fields",
                       "key 'fields' must be an array",
                       "message fields value is not an array");

    std::string non_table_field = base.substr(0, field_start);
    non_table_field += "fields = [\"wrong\"]\n";
    expect_parse_error(tests, non_table_field, "FFSCHEMA004",
                       "schema.messages.event.fields[0]",
                       "each fields entry must be a table",
                       "message fields array entry is not a table");
  }

  struct optional_case {
    std::string_view text;
    std::string_view replacement;
    std::string_view object;
    std::string_view message;
    std::string_view description;
  };
  constexpr std::array cases{
      optional_case{"description = \"Focused validation fixture\"",
                    "description = 1", "schema.description",
                    "must be a string",
                    "top-level description has wrong TOML type"},
      optional_case{"logical = \"ascii\"", "logical = 1",
                    "schema.types[0].logical", "must be a string",
                    "type logical has wrong TOML type"},
      optional_case{"logical = \"ascii\"\n",
                    "logical = \"ascii\"\nscale = \"one\"\n",
                    "schema.types[0].scale", "must be an integer",
                    "type scale has wrong TOML type"},
      optional_case{"description = \"Payload alias\"", "description = 1",
                    "schema.types[0].description", "must be a string",
                    "type description has wrong TOML type"},
      optional_case{"type = \"A\"", "type = 65",
                    "schema.messages[0].type", "must be a string",
                    "message type has wrong TOML type"},
      optional_case{"description = \"Event\"", "description = 1",
                    "schema.messages[0].description", "must be a string",
                    "message description has wrong TOML type"},
      optional_case{"spec_section = \"fixture\"\nspec_page = 1\n\n"
                    "[[messages.fields]]",
                    "spec_section = 1\nspec_page = 1\n\n"
                    "[[messages.fields]]",
                    "schema.messages[0].spec_section", "must be a string",
                    "message spec_section has wrong TOML type"},
      optional_case{"spec_page = 1\n\n[[messages.fields]]",
                    "spec_page = \"one\"\n\n[[messages.fields]]",
                    "schema.messages[0].spec_page", "must be an integer",
                    "message spec_page has wrong TOML type"},
      optional_case{"role = \"discriminator\"", "role = 1",
                    "schema.messages.event.fields[0].role",
                    "must be a string",
                    "field role has wrong TOML type"},
      optional_case{"value = \"A\"", "value = 65",
                    "schema.messages.event.fields[0].value",
                    "must be a string",
                    "field discriminator value has wrong TOML type"},
      optional_case{"description = \"Discriminator\"",
                    "description = 1",
                    "schema.messages.event.fields[0].description",
                    "must be a string",
                    "field description has wrong TOML type"},
      optional_case{"description = \"Discriminator\"\n"
                    "spec_section = \"fixture\"",
                    "description = \"Discriminator\"\n"
                    "spec_section = 1",
                    "schema.messages.event.fields[0].spec_section",
                    "must be a string",
                    "field spec_section has wrong TOML type"},
      optional_case{"description = \"Discriminator\"\n"
                    "spec_section = \"fixture\"\nspec_page = 1",
                    "description = \"Discriminator\"\n"
                    "spec_section = \"fixture\"\nspec_page = \"one\"",
                    "schema.messages.event.fields[0].spec_page",
                    "must be an integer",
                    "field spec_page has wrong TOML type"},
  };
  for (const optional_case& item : cases) {
    expect_parse_error(
        tests,
        support::replace_once(tests, base, item.text, item.replacement,
                              item.description),
        "FFSCHEMA004", item.object, item.message, item.description);
  }

  expect_parse_error(
      tests,
      support::replace_once(
          tests, base,
          "description = \"Discriminator\"\n"
          "spec_section = \"fixture\"\nspec_page = 1",
          "description = \"Discriminator\"\n"
          "spec_section = \"fixture\"\nspec_page = -1",
          "negative field spec_page"),
      "FFSCHEMA032", "schema.messages.event.fields[0].spec_page",
      "spec_page must be non-negative",
      "negative field metadata integer");
}

void test_positive_grammar(support::suite& tests) {
  const auto parsed = compiler::parse_schema_toml(
      support::valid_schema_toml, "audit\\valid_schema.toml");
  tests.check(parsed.has_value(),
              "complete schema grammar fixture parses");
  if (parsed) {
    tests.check(compiler::validate_schema(*parsed).has_value(),
                "complete schema grammar fixture validates");
  }

  std::string no_types{support::valid_schema_toml};
  const std::size_t type_start = no_types.find("[[types]]");
  const std::size_t message_start = no_types.find("[[messages]]");
  tests.check(type_start != std::string::npos &&
                  message_start != std::string::npos &&
                  type_start < message_start,
              "optional types mutation range exists");
  if (type_start != std::string::npos &&
      message_start != std::string::npos && type_start < message_start) {
    no_types.erase(type_start, message_start - type_start);
  }
  no_types = support::replace_once(tests, std::move(no_types),
                                   "type = \"payload_type\"",
                                   "type = \"alpha\"",
                                   "schema without user types");
  const auto no_types_result =
      compiler::parse_schema_toml(no_types, "audit/no_types.toml");
  tests.check(no_types_result.has_value(),
              "schema accepts an empty user-type set");
  if (no_types_result) {
    tests.check(no_types_result->types.empty(),
                "omitted types lower to an empty type set");
    tests.check(compiler::validate_schema(*no_types_result).has_value(),
                "schema without user types validates");
  }

  const std::string defaults = support::erase_once(
      tests, std::string{support::valid_schema_toml},
      "logical = \"ascii\"\n", "omitted logical default");
  const auto defaults_result =
      compiler::parse_schema_toml(defaults, "audit/default_logical.toml");
  tests.check(defaults_result.has_value(),
              "type logical key may be omitted");
  if (defaults_result) {
    const auto resolved = compiler::validate_schema(*defaults_result);
    tests.check(resolved.has_value(),
                "omitted ascii logical defaults successfully");
    if (resolved) {
      tests.check(resolved->types.front().logical ==
                      compiler::logical_kind::ascii,
                  "ascii physical kind defaults to ascii logical kind");
    }
  }

  const std::string hexadecimal = support::replace_once(
      tests, std::string{support::valid_schema_toml}, "width = 1\nlogical",
      "width = 0x1\nlogical", "TOML hexadecimal integer");
  const auto hexadecimal_result =
      compiler::parse_schema_toml(hexadecimal, "audit/hex.toml");
  tests.check(hexadecimal_result.has_value() &&
                  compiler::validate_schema(*hexadecimal_result).has_value(),
              "representable non-negative TOML integer literal is accepted");

  const std::string maximum_integer = support::replace_once(
      tests, std::string{support::valid_schema_toml},
      "spec_page = 1\n\n[[messages.fields]]",
      "spec_page = 9223372036854775807\n\n[[messages.fields]]",
      "maximum signed 64-bit TOML integer");
  const auto maximum_integer_result = compiler::parse_schema_toml(
      maximum_integer, "audit/maximum_integer.toml");
  tests.check(
      maximum_integer_result.has_value() &&
          compiler::validate_schema(*maximum_integer_result).has_value(),
      "maximum representable non-negative TOML integer is accepted");
}

}  // namespace

int main() {
  support::suite tests;
  test_closed_grammar(tests);
  test_required_top_level_keys(tests);
  test_required_nested_keys(tests);
  test_toml_types_and_duplicates(tests);
  test_table_shapes_and_optional_value_types(tests);
  test_positive_grammar(tests);
  return tests.finish("schema grammar");
}
