#include <array>
#include <string>
#include <string_view>
#include <utility>

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

void test_parser_preflight(support::suite& tests) {
  constexpr std::string_view malformed_bom{"\xEF\xBB\xBF[!\n", 6U};
  for (const std::string_view malformed : {std::string_view{"[[\n"}, std::string_view{"[[{\n"},
                                           std::string_view{"[!\n"}, malformed_bom}) {
    tests.expect_error(compiler::parse_schema_toml(malformed, "audit/header.toml"), "FFTOML001",
                       "schema", "table header key",
                       "malformed table header is rejected before parser preconditions",
                       std::string_view{"audit/header.toml"});
  }

  std::string invalid_utf8{"name = \""};
  invalid_utf8.push_back(static_cast<char>(0xFFU));
  invalid_utf8.append("\"\n");
  tests.expect_error(compiler::parse_schema_toml(invalid_utf8, "audit/utf8.toml"), "FFTOML001",
                     "schema", "valid UTF-8", "malformed UTF-8 is rejected before parsing",
                     std::string_view{"audit/utf8.toml"});

  std::string invalid_utf8_position{"\xEF\xBB\xBFname = \"\xE2\x82\xAC"};
  invalid_utf8_position.push_back(static_cast<char>(0xFFU));
  invalid_utf8_position.append("\"\n");
  const auto positioned_error =
      compiler::parse_schema_toml(invalid_utf8_position, "audit/utf8-position.toml");
  tests.check(!positioned_error &&
                  positioned_error.error().position == compiler::source_position{1U, 10U},
              "malformed UTF-8 position counts Unicode code points after the BOM");

  constexpr std::string_view unicode_header_position{
      "\xEF\xBB\xBF[[\"\xE2\x82\xAC\".cost\xE2\x82\xAC]]\n"};
  const auto header_position_error =
      compiler::parse_schema_toml(unicode_header_position, "audit/header-position.toml");
  tests.check(!header_position_error &&
                  header_position_error.error().position == compiler::source_position{1U, 11U},
              "table-header diagnostics count Unicode code points after the BOM");

  constexpr std::string_view unicode_key_position{
      "\xEF\xBB\xBF\"\xE2\x82\xAC\".cost\xE2\x82\xAC = 1\n"};
  const auto key_position_error =
      compiler::parse_schema_toml(unicode_key_position, "audit/key-position.toml");
  tests.check(!key_position_error &&
                  key_position_error.error().position == compiler::source_position{1U, 9U},
              "bare-key diagnostics count Unicode code points after the BOM");

  constexpr std::string_view unicode_array_position{"\xEF\xBB\xBF"
                                                    "a = [\"\xE2\x82\xAC\",}\n"};
  const auto array_position_error =
      compiler::parse_schema_toml(unicode_array_position, "audit/array-position.toml");
  tests.check(!array_position_error &&
                  array_position_error.error().position == compiler::source_position{1U, 10U},
              "array-brace diagnostics count Unicode code points after the BOM");

  constexpr std::string_view invalid_single_line_transition{"name = \"bad\\\n\"\nallowed = [}\n"};
  const auto single_line_error =
      compiler::parse_schema_toml(invalid_single_line_transition, "audit/single-line.toml");
  tests.check(!single_line_error && single_line_error.error().position.has_value() &&
                  single_line_error.error().position->line == 1U,
              "invalid single-line strings retain the first parser diagnostic");

  constexpr std::string_view invalid_header_string_transition{"[\"bad\\\n\"x\xE2\x82\xAC = 1\n"};
  const auto header_string_error =
      compiler::parse_schema_toml(invalid_header_string_transition, "audit/header-string.toml");
  tests.check(!header_string_error && header_string_error.error().position.has_value() &&
                  header_string_error.error().position->line == 1U,
              "invalid quoted table keys retain the first parser diagnostic");

  constexpr std::string_view nonascii_bare_key{"key\xEF\xBB\x91 = 1\n"};
  tests.expect_error(compiler::parse_schema_toml(nonascii_bare_key, "audit/nonascii-key.toml"),
                     "FFTOML001", "schema", "outside strings and comments",
                     "non-ASCII bare keys are rejected before parser whitespace classification",
                     std::string_view{"audit/nonascii-key.toml"});

  constexpr std::string_view nonascii_header{"[[messages\xD2\x99ields]]\n"};
  tests.expect_error(compiler::parse_schema_toml(nonascii_header, "audit/nonascii-header.toml"),
                     "FFTOML001", "schema", "outside strings and comments",
                     "non-ASCII table keys are rejected before parser whitespace classification",
                     std::string_view{"audit/nonascii-header.toml"});

  const std::string unmatched_array_brace =
      support::replace_once(tests, std::string{support::valid_schema_toml}, "allowed = [\"X\"]",
                            "allowed = [}", "unmatched array brace");
  tests.expect_error(compiler::parse_schema_toml(unmatched_array_brace, "audit/array-brace.toml"),
                     "FFTOML001", "schema", "unmatched '}' inside array",
                     "array terminator is rejected before parser value preconditions",
                     std::string_view{"audit/array-brace.toml"});

  std::string bom_schema{"\xEF\xBB\xBF"};
  bom_schema.append(support::valid_schema_toml);
  const auto bom_result = compiler::parse_schema_toml(bom_schema, "audit/bom.toml");
  tests.check(bom_result.has_value() && compiler::validate_schema(*bom_result).has_value(),
              "UTF-8 BOM passes parser preflight");

  const std::string unicode = support::replace_once(
      tests, std::string{support::valid_schema_toml},
      "description = \"Focused validation fixture\"",
      "description = \"Cost \xE2\x82\xAC\" # currency \xE2\x82\xAC", "Unicode string and comment");
  const auto unicode_result = compiler::parse_schema_toml(unicode, "audit/unicode.toml");
  tests.check(unicode_result.has_value() && compiler::validate_schema(*unicode_result).has_value(),
              "Unicode strings and comments pass parser preflight");

  const std::string quoted =
      support::replace_once(tests, std::string{support::valid_schema_toml}, "[[types]]",
                            "[[\"types\"]]", "quoted table key");
  const auto quoted_result = compiler::parse_schema_toml(quoted, "audit/quoted.toml");
  tests.check(quoted_result.has_value() && compiler::validate_schema(*quoted_result).has_value(),
              "quoted table key passes parser preflight");

  const std::string unicode_quoted =
      support::replace_once(tests, std::string{support::valid_schema_toml}, "[[types]]",
                            "[[\"typ\xE2\x82\xACs\"]]", "Unicode quoted table key");
  const auto unicode_quoted_result =
      compiler::parse_schema_toml(unicode_quoted, "audit/unicode-key.toml");
  tests.check(!unicode_quoted_result && unicode_quoted_result.error().code == "FFSCHEMA002",
              "Unicode quoted table keys reach schema validation");

  const std::string unicode_header_comment =
      support::replace_once(tests, std::string{support::valid_schema_toml}, "[[types]]",
                            "[[types]] # currency \xE2\x82\xAC", "Unicode table-header comment");
  const auto unicode_header_comment_result =
      compiler::parse_schema_toml(unicode_header_comment, "audit/unicode-comment.toml");
  tests.check(unicode_header_comment_result.has_value() &&
                  compiler::validate_schema(*unicode_header_comment_result).has_value(),
              "Unicode table-header comments pass parser preflight");

  const std::string dotted =
      support::replace_once(tests, std::string{support::valid_schema_toml}, "[[messages.fields]]",
                            "[[messages.\"fields\"]]", "quoted dotted table key");
  const auto dotted_result = compiler::parse_schema_toml(dotted, "audit/dotted.toml");
  tests.check(dotted_result.has_value() && compiler::validate_schema(*dotted_result).has_value(),
              "quoted dotted table key passes parser preflight");

  const std::string multiline = support::replace_once(
      tests, std::string{support::valid_schema_toml},
      "description = \"Focused validation fixture\"",
      "description = \"\"\"\n[[\n[[{\n[!\n\"\"\"\n# [[{", "multiline header-like text");
  const auto multiline_result = compiler::parse_schema_toml(multiline, "audit/multiline.toml");
  tests.check(multiline_result.has_value() &&
                  compiler::validate_schema(*multiline_result).has_value(),
              "comments and multiline strings are ignored by header preflight");

  for (const auto& [name, quoted_description] :
       std::array<std::pair<std::string_view, std::string_view>, 4U>{
           std::pair{"basic four-quote", "description = \"\"\"Quoted\"\"\"\""},
           std::pair{"basic five-quote", "description = \"\"\"Quoted\"\"\"\"\""},
           std::pair{"literal four-quote", "description = '''Quoted''''"},
           std::pair{"literal five-quote", "description = '''Quoted'''''"}}) {
    const std::string valid_quote_run =
        support::replace_once(tests, std::string{support::valid_schema_toml},
                              "description = \"Focused validation fixture\"", quoted_description,
                              std::string{name} + " multiline quote run");
    const auto valid_quote_run_result =
        compiler::parse_schema_toml(valid_quote_run, "audit/quote-run.toml");
    tests.check(valid_quote_run_result.has_value() &&
                    compiler::validate_schema(*valid_quote_run_result).has_value(),
                std::string{name} + " multiline strings accept a quote before the delimiter");

    std::string unicode_key_description{quoted_description};
    unicode_key_description.append("\n\"cost\xE2\x82\xAC\" = 1");
    const std::string unicode_key_after_quote_run = support::replace_once(
        tests, std::string{support::valid_schema_toml},
        "description = \"Focused validation fixture\"", unicode_key_description,
        std::string{name} + " multiline Unicode key quote run");
    tests.expect_error(
        compiler::parse_schema_toml(unicode_key_after_quote_run, "audit/quote-run-key.toml"),
        "FFSCHEMA002", "schema.cost\xE2\x82\xAC", "unknown key",
        std::string{name} + " multiline close exposes Unicode quoted keys to schema validation",
        std::string_view{"audit/quote-run-key.toml"});

    std::string malformed_description{quoted_description};
    malformed_description.append("\nallowed = [}");
    const std::string malformed_after_quote_run =
        support::replace_once(tests, std::string{support::valid_schema_toml},
                              "description = \"Focused validation fixture\"", malformed_description,
                              std::string{name} + " multiline guard quote run");
    tests.expect_error(
        compiler::parse_schema_toml(malformed_after_quote_run, "audit/quote-run-guard.toml"),
        "FFTOML001", "schema", "unmatched '}' inside array",
        std::string{name} + " multiline close does not hide malformed arrays",
        std::string_view{"audit/quote-run-guard.toml"});
  }

  const std::string nested_array = support::replace_once(
      tests, std::string{support::valid_schema_toml}, "[[types]]",
      "unknown_top = [\n  [{ value = \"ok\" }]\n]\n\n[[types]]", "nested array at line start");
  tests.expect_error(compiler::parse_schema_toml(nested_array, "audit/array.toml"), "FFSCHEMA002",
                     "schema.unknown_top", "unknown key",
                     "nested arrays are not mistaken for table headers",
                     std::string_view{"audit/array.toml"});
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
  test_parser_preflight(tests);
  test_table_shapes_and_optional_value_types(tests);
  test_positive_grammar(tests);
  return tests.finish("schema grammar");
}
