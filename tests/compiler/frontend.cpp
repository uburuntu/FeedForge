#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "diagnostics.hpp"
#include "feedforge/version.hpp"
#include "ir.hpp"
#include "lower.hpp"
#include "model.hpp"
#include "parse_toml.hpp"
#include "sha256.hpp"

namespace {

namespace compiler = feedforge::compiler;
namespace fs = std::filesystem;

int failures = 0;

void check(const bool condition, const std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

[[nodiscard]] std::string read_text(const fs::path& path) {
  std::ifstream input{path, std::ios::binary};
  std::ostringstream contents;
  contents << input.rdbuf();
  return std::move(contents).str();
}

[[nodiscard]] std::string replaced(std::string input,
                                   const std::string_view from,
                                   const std::string_view to) {
  const std::size_t position = input.find(from);
  check(position != std::string::npos, "test mutation source text was found");
  if (position != std::string::npos) {
    input.replace(position, from.size(), to);
  }
  return input;
}

void expect_parse_code(const compiler::result<compiler::schema_source>& result,
                       const std::string_view code,
                       const std::string_view description) {
  check(!result, description);
  if (!result) {
    check(result.error().code == code,
          std::string(description) + " diagnostic code");
  }
}

void expect_parse_code(
    const compiler::result<compiler::pipeline_source>& result,
    const std::string_view code, const std::string_view description) {
  check(!result, description);
  if (!result) {
    check(result.error().code == code,
          std::string(description) + " diagnostic code");
  }
}

template <class Mutation>
void expect_schema_code(const compiler::schema_source& base,
                        Mutation mutation, const std::string_view code,
                        const std::string_view description) {
  compiler::schema_source candidate = base;
  mutation(candidate);
  const auto result = compiler::validate_schema(candidate);
  check(!result, description);
  if (!result) {
    check(result.error().code == code,
          std::string(description) + " diagnostic code");
  }
}

template <class Mutation>
void expect_pipeline_code(const compiler::pipeline_source& base,
                          const compiler::resolved_schema& schema,
                          Mutation mutation, const std::string_view code,
                          const std::string_view description) {
  compiler::pipeline_source candidate = base;
  mutation(candidate);
  const auto result = compiler::validate_pipeline(candidate, schema);
  check(!result, description);
  if (!result) {
    check(result.error().code == code,
          std::string(description) + " diagnostic code");
  }
}

void test_names() {
  for (const std::string_view accepted :
       {"a", "a0", "a_b", "z9_", "portable_checked", "reflexpr",
        "atomic_cancel"}) {
    check(compiler::is_valid_source_name(accepted),
          std::string("accepted source name ") + std::string(accepted));
  }
  for (const std::string_view rejected :
       {"", "A", "_a", "1a", "a-b", "a__b", "class", "co_await"}) {
    check(!compiler::is_valid_source_name(rejected),
          std::string("rejected source name ") + std::string(rejected));
  }
  for (const std::string_view accepted : {"feedforge", "FeedForge", "a_1"}) {
    check(compiler::is_valid_namespace_component(accepted),
          std::string("accepted namespace component ") +
              std::string(accepted));
  }
  for (const std::string_view rejected :
       {"", "_feedforge", "1feedforge", "a__b", "class", "a-b"}) {
    check(!compiler::is_valid_namespace_component(rejected),
          std::string("rejected namespace component ") +
              std::string(rejected));
  }
}

void test_sha256_and_json() {
  check(compiler::sha256_hex("") ==
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855",
        "SHA-256 empty-string NIST vector");
  check(compiler::sha256_hex("abc") ==
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad",
        "SHA-256 abc NIST vector");
  check(compiler::sha256_hex(
            "abcdbcdecdefdefgefghfghighijhijk"
            "ijkljklmklmnlmnomnopnopq") ==
            "248d6a61d20638b8e5c026930c3e6039"
            "a33ce45964ff2167f6ecedd419db06c1",
        "SHA-256 multi-block NIST vector");
  check(compiler::escape_json_string(
            std::string_view{"\"\\" "\b\f\n\r\t\x01", 8U}) ==
            "\\\"\\\\\\b\\f\\n\\r\\t\\u0001",
        "RFC 8259 control and quote escaping");
}

void test_structural_parsing(const std::string& schema_text,
                             const std::string& pipeline_text) {
  expect_parse_code(
      compiler::parse_schema_toml("format_version = [", "bad\\schema.toml"),
      "FFTOML001", "malformed TOML rejected");
  auto malformed =
      compiler::parse_schema_toml("format_version = [", "bad\\schema.toml");
  if (!malformed) {
    check(malformed.error().path == "bad/schema.toml",
          "diagnostic preserves supplied generic path spelling");
    check(malformed.error().position.has_value(),
          "TOML syntax diagnostic has source position");
  }
  expect_parse_code(
      compiler::parse_schema_toml(
          replaced(schema_text, "format_version = 1\n", ""), "schema.toml"),
      "FFSCHEMA001", "missing schema version rejected");
  expect_parse_code(
      compiler::parse_schema_toml(
          replaced(schema_text, "format_version = 1",
                   "format_version = \"one\""),
          "schema.toml"),
      "FFSCHEMA004", "schema value with wrong TOML type rejected");
  expect_parse_code(
      compiler::parse_schema_toml(
          replaced(schema_text, "description = ",
                   "unknown_top_level = true\ndescription = "),
          "schema.toml"),
      "FFSCHEMA002", "unknown schema key rejected");
  expect_parse_code(
      compiler::parse_schema_toml(
          replaced(schema_text, "spec_page = 1", "spec_page = -1"),
          "schema.toml"),
      "FFSCHEMA032", "negative schema metadata integer rejected");
  expect_parse_code(
      compiler::parse_schema_toml(
          replaced(schema_text, "name = \"test_feed\"",
                   "name = \"test_feed\"\nname = \"duplicate\""),
          "schema.toml"),
      "FFTOML001", "duplicate TOML key rejected");

  expect_parse_code(
      compiler::parse_pipeline_toml(
          replaced(pipeline_text, "format_version = 1\n", ""),
          "pipeline.toml"),
      "FFPIPE001", "missing pipeline version rejected");
  expect_parse_code(
      compiler::parse_pipeline_toml(
          replaced(pipeline_text, "namespace = ",
                   "unknown_top_level = true\nnamespace = "),
          "pipeline.toml"),
      "FFPIPE002", "unknown pipeline key rejected");
  expect_parse_code(
      compiler::parse_pipeline_toml(
          replaced(pipeline_text, "profile = \"portable_checked\"",
                   "profile = 1"),
          "pipeline.toml"),
      "FFPIPE004", "pipeline value with wrong TOML type rejected");
  expect_parse_code(
      compiler::parse_pipeline_toml(
          replaced(pipeline_text, "schema = \"test_feed\"\n", ""),
          "pipeline.toml"),
      "FFPIPE003", "missing pipeline key rejected");
}

void test_schema_validation(const compiler::schema_source& base) {
  expect_schema_code(
      base, [](auto& value) { value.format_version = 2; }, "FFSCHEMA001",
      "unsupported schema version");
  expect_schema_code(
      base, [](auto& value) { value.name = "class"; }, "FFSCHEMA005",
      "reserved schema identifier");
  expect_schema_code(
      base,
      [](auto& value) { value.types.push_back(value.types.front()); },
      "FFSCHEMA006", "duplicate semantic type");
  expect_schema_code(
      base, [](auto& value) { value.types.front().name = "u8"; },
      "FFSCHEMA007", "built-in type shadow");
  expect_schema_code(
      base,
      [](auto& value) { value.messages.push_back(value.messages.front()); },
      "FFSCHEMA008", "duplicate message name");
  expect_schema_code(
      base,
      [](auto& value) {
        value.messages[1].type = "A";
        value.messages[1].fields[0].value = "A";
      },
      "FFSCHEMA009", "duplicate message discriminator");
  expect_schema_code(
      base,
      [](auto& value) {
        value.messages[0].fields.push_back(value.messages[0].fields.front());
      },
      "FFSCHEMA010", "duplicate field name");
  expect_schema_code(
      base, [](auto& value) { value.wire_endian = "little"; },
      "FFSCHEMA011", "unsupported wire endian");
  expect_schema_code(
      base, [](auto& value) { value.types.front().kind = "float"; },
      "FFSCHEMA012", "unsupported physical kind");
  expect_schema_code(
      base,
      [](auto& value) { value.types.front().logical = "currency"; },
      "FFSCHEMA013", "unsupported logical kind");
  expect_schema_code(
      base, [](auto& value) { value.types.front().width = 3; },
      "FFSCHEMA014", "unsupported integer width");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields[1].width = 0; },
      "FFSCHEMA014", "non-positive field width");
  expect_schema_code(
      base, [](auto& value) { value.types.front().scale = 19; },
      "FFSCHEMA015", "decimal scale outside range");
  expect_schema_code(
      base,
      [](auto& value) {
        value.types.front().logical = "raw_unsigned";
        value.types.front().scale = 4;
      },
      "FFSCHEMA015", "scale on non-decimal type");
  expect_schema_code(
      base,
      [](auto& value) {
        value.types.front().width = 2;
        value.types.front().logical = "decimal";
      },
      "FFSCHEMA016", "incompatible physical and logical type");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].type = " "; },
      "FFSCHEMA017", "non-printable message type");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].size = 0; }, "FFSCHEMA018",
      "non-positive message size");
  expect_schema_code(
      base,
      [](auto& value) { value.messages[0].fields[1].type = "missing"; },
      "FFSCHEMA019", "undeclared field type");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields[1].width = 4; },
      "FFSCHEMA020", "fixed-width type disagreement");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields[1].offset = -1; },
      "FFSCHEMA021", "negative field offset");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields[4].width = 2; },
      "FFSCHEMA022", "field beyond message boundary");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields[3].offset = 6; },
      "FFSCHEMA023", "overlapping fields");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields[1].offset = 2; },
      "FFSCHEMA024", "undeclared byte gap");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields.pop_back(); },
      "FFSCHEMA025", "incomplete final byte coverage");
  expect_schema_code(
      base,
      [](auto& value) {
        value.messages[0].fields[0].role.reset();
        value.messages[0].fields[0].value.reset();
      },
      "FFSCHEMA026", "missing discriminator");
  expect_schema_code(
      base, [](auto& value) { value.discriminator_width = 2; },
      "FFSCHEMA027", "invalid schema discriminator layout");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields[0].offset = 1; },
      "FFSCHEMA027", "invalid field discriminator layout");
  expect_schema_code(
      base,
      [](auto& value) { value.messages[0].fields[0].value = "Z"; },
      "FFSCHEMA028", "discriminator value mismatch");
  expect_schema_code(
      base, [](auto& value) { value.messages.clear(); }, "FFSCHEMA030",
      "schema without messages");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields.clear(); },
      "FFSCHEMA031", "message without fields");
  expect_schema_code(
      base, [](auto& value) { value.messages[0].fields[0].type = "u8"; },
      "FFSCHEMA033", "non-ASCII discriminator type");
  expect_schema_code(
      base,
      [](auto& value) {
        value.messages[0].fields[3].allowed = {"BUY"};
      },
      "FFSCHEMA034", "allowed metadata width mismatch");
  expect_schema_code(
      base,
      [](auto& value) {
        value.messages[0].fields[1].role = "payload";
      },
      "FFSCHEMA035", "unsupported field role");
  expect_schema_code(
      base,
      [](auto& value) { value.messages[0].fields[1].value = "A"; },
      "FFSCHEMA035", "value on non-discriminator");

  compiler::schema_source logicals = base;
  logicals.types.push_back(
      {{}, "timestamp", "uint", 6, "timestamp_ns", std::nullopt});
  logicals.types.push_back(
      {{}, "stock_locate", "uint", 2, "stock_locate", std::nullopt});
  logicals.types.push_back(
      {{}, "tracking_number", "uint", 2, "tracking_number", std::nullopt});
  logicals.types.push_back({{}, "order_reference", "uint", 8,
                            "order_reference_number", std::nullopt});
  logicals.types.push_back(
      {{}, "match_id", "uint", 8, "match_number", std::nullopt});
  logicals.types.push_back(
      {{}, "shares", "uint", 4, "share_count", std::nullopt});
  logicals.types.push_back(
      {{}, "symbol", "ascii", 8, "ascii", std::nullopt});
  check(compiler::validate_schema(logicals).has_value(),
        "all supported logical/physical combinations validate");
}

void test_pipeline_validation(const compiler::pipeline_source& base,
                              const compiler::resolved_schema& schema) {
  expect_pipeline_code(
      base, schema, [](auto& value) { value.format_version = 2; },
      "FFPIPE001", "unsupported pipeline version");
  expect_pipeline_code(
      base, schema, [](auto& value) { value.name = "class"; }, "FFPIPE005",
      "reserved pipeline identifier");
  expect_pipeline_code(
      base, schema, [](auto& value) { value.cpp_namespace = "ok::_hidden"; },
      "FFPIPE006", "reserved namespace component");
  expect_pipeline_code(
      base, schema, [](auto& value) { value.cpp_namespace = "ok:::bad"; },
      "FFPIPE006", "malformed namespace separator");
  expect_pipeline_code(
      base, schema, [](auto& value) { value.schema = "other_schema"; },
      "FFPIPE007", "pipeline schema mismatch");
  expect_pipeline_code(
      base, schema, [](auto& value) { value.profile = "fast"; },
      "FFPIPE008", "unsupported profile");
  expect_pipeline_code(
      base, schema, [](auto& value) { value.unknown_messages = "ignore"; },
      "FFPIPE009", "unsupported unknown-message policy");
  expect_pipeline_code(
      base, schema, [](auto& value) { value.unselected_messages = "error"; },
      "FFPIPE009", "unsupported unselected-message policy");
  expect_pipeline_code(
      base, schema, [](auto& value) { value.projections.clear(); },
      "FFPIPE010", "pipeline without emits");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections.front().source = "AA"; },
      "FFPIPE011", "multi-byte source");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections.front().source = "Z"; },
      "FFPIPE012", "unknown source");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections[1].source = "U"; }, "FFPIPE013",
      "duplicate source selection");
  expect_pipeline_code(
      base, schema,
      [](auto& value) {
        value.projections[1].event = value.projections[0].event;
      },
      "FFPIPE014", "duplicate event name");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections.front().fields.clear(); },
      "FFPIPE015", "emit without fields");
  expect_pipeline_code(
      base, schema,
      [](auto& value) {
        value.projections.front().fields = {"*", "quantity"};
      },
      "FFPIPE016", "mixed wildcard and explicit fields");
  expect_pipeline_code(
      base, schema,
      [](auto& value) {
        value.projections[1].fields = {"price", "price"};
      },
      "FFPIPE017", "duplicate projected field");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections[1].fields = {"missing"}; },
      "FFPIPE018", "unknown projected field");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections[1].fields = {"message_type"}; },
      "FFPIPE019", "explicit discriminator projection");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections[1].fields = {"padding"}; },
      "FFPIPE019", "explicit reserved projection");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections.front().event = "class"; },
      "FFPIPE005", "reserved event identifier");
  expect_pipeline_code(
      base, schema,
      [](auto& value) { value.projections.front().event = "decoder"; },
      "FFPIPE021", "event colliding with generated decoder declaration");

  compiler::resolved_schema no_representation = schema;
  no_representation.messages[0].fields[2].logical =
      compiler::logical_kind::timestamp_ns;
  expect_pipeline_code(
      base, no_representation, [](auto&) {}, "FFPIPE020",
      "projected field without C++20 representation");

  compiler::resolved_schema generated_field_collision = schema;
  generated_field_collision.messages[0].fields[2].name = "event_name";
  expect_pipeline_code(
      base, generated_field_collision,
      [](auto& value) {
        value.projections[1].fields[0] = "event_name";
      },
      "FFPIPE021", "field colliding with generated event declaration");

  compiler::resolved_schema no_wildcard_fields = schema;
  for (auto& field : no_wildcard_fields.messages[2].fields) {
    field.projectable = false;
  }
  expect_pipeline_code(
      base, no_wildcard_fields, [](auto&) {}, "FFPIPE015",
      "wildcard resolving to no projectable fields");
}

void test_lowering(const compiler::schema_source& schema_source,
                   const compiler::pipeline_source& pipeline_source,
                   const compiler::schema_source& reordered_schema_source,
                   const compiler::pipeline_source& reordered_pipeline_source) {
  const auto schema = compiler::validate_schema(schema_source);
  const auto pipeline =
      schema ? compiler::validate_pipeline(pipeline_source, *schema)
             : compiler::result<compiler::resolved_pipeline>{
                   std::unexpected(compiler::diagnostic{})};
  const auto reordered_schema =
      compiler::validate_schema(reordered_schema_source);
  const auto reordered_pipeline =
      reordered_schema
          ? compiler::validate_pipeline(reordered_pipeline_source,
                                        *reordered_schema)
          : compiler::result<compiler::resolved_pipeline>{
                std::unexpected(compiler::diagnostic{})};
  check(schema.has_value() && pipeline.has_value() &&
            reordered_schema.has_value() && reordered_pipeline.has_value(),
        "positive fixtures validate before lowering");
  if (!schema || !pipeline || !reordered_schema || !reordered_pipeline) {
    return;
  }

  const compiler::ffir_v1 ir =
      compiler::lower_to_ffir(*schema, *pipeline, feedforge::version_string);
  const compiler::ffir_v1 reordered = compiler::lower_to_ffir(
      *reordered_schema, *reordered_pipeline, feedforge::version_string);
  const std::string json = compiler::canonical_json(ir);
  check(json ==
            read_text(fs::path{FEEDFORGE_COMPILER_FIXTURE_DIR} /
                      "valid.ffir.json"),
        "canonical FFIR JSON matches exact golden bytes");
  check(json == compiler::canonical_json(reordered),
        "comments and non-semantic TOML ordering preserve canonical FFIR");
  check(ir.schema.fingerprint == reordered.schema.fingerprint,
        "schema semantic fingerprint ignores source ordering");
  check(ir.pipeline.fingerprint == reordered.pipeline.fingerprint,
        "pipeline semantic fingerprint ignores emit ordering");
  check(json.ends_with('\n'), "canonical FFIR JSON has newline at EOF");
  check(json.find(FEEDFORGE_COMPILER_FIXTURE_DIR) == std::string::npos,
        "canonical FFIR omits source paths");
  check(ir.schema.messages.size() == 3U &&
            std::ranges::is_sorted(ir.schema.messages, {},
                                   &compiler::ffir_message_v1::discriminator),
        "messages lower in unsigned discriminator order");
  check(ir.pipeline.events.size() == 2U &&
            ir.pipeline.events[0].source_discriminator <
                ir.pipeline.events[1].source_discriminator,
        "events lower in unsigned discriminator order");
  check(ir.pipeline.events[0].fields.size() == 2U &&
            ir.pipeline.events[0].fields[0].name == "price" &&
            ir.pipeline.events[0].fields[1].name == "side",
        "explicit projected fields retain declaration order");
  check(ir.pipeline.events[1].fields.size() == 2U &&
            ir.pipeline.events[1].fields[0].name == "quantity" &&
            ir.pipeline.events[1].fields[1].name == "code",
        "wildcard fields expand in wire-offset order");
  check(ir.schema.fingerprint.size() == 64U &&
            ir.pipeline.fingerprint.size() == 64U,
        "semantic fingerprints are lowercase SHA-256 hex");

  compiler::resolved_pipeline reordered_fields = *pipeline;
  std::ranges::swap(reordered_fields.projections[0].fields[0],
                    reordered_fields.projections[0].fields[1]);
  const compiler::ffir_v1 field_order_ir =
      compiler::lower_to_ffir(*schema, reordered_fields,
                              feedforge::version_string);
  check(ir.pipeline.fingerprint != field_order_ir.pipeline.fingerprint,
        "projected field order changes pipeline fingerprint");

  compiler::resolved_schema changed_provenance = *schema;
  changed_provenance.document_revision = "different-provenance";
  const compiler::ffir_v1 provenance_ir =
      compiler::lower_to_ffir(changed_provenance, *pipeline,
                              feedforge::version_string);
  check(ir.schema.fingerprint == provenance_ir.schema.fingerprint,
        "document revision provenance is omitted from schema hash input");
  check(compiler::canonical_schema_semantics_json(ir).find("fingerprint") ==
            std::string::npos &&
            compiler::canonical_schema_semantics_json(ir).find(
                "document_revision") == std::string::npos,
        "schema hash input omits fingerprint and provenance");
  check(compiler::canonical_pipeline_semantics_json(ir).find("fingerprint") ==
            std::string::npos,
        "pipeline hash input omits fingerprint");
}

void test_diagnostic_format() {
  const compiler::diagnostic problem = compiler::make_diagnostic(
      "FFTEST001", "fixtures\\schema.toml",
      compiler::source_mark{compiler::source_position{12U, 7U}},
      "schema.messages.test", "example problem", "fix the fixture");
  check(compiler::format_diagnostic(problem) ==
            "FFTEST001 fixtures/schema.toml:12:7: "
            "schema.messages.test: example problem\nhint: fix the fixture",
        "structured diagnostic rendering is stable");
}

}  // namespace

int main() {
  const fs::path fixtures{FEEDFORGE_COMPILER_FIXTURE_DIR};
  const std::string schema_text = read_text(fixtures / "valid_schema.toml");
  const std::string pipeline_text =
      read_text(fixtures / "valid_pipeline.toml");
  const auto schema =
      compiler::parse_schema_toml(schema_text, "fixtures/valid_schema.toml");
  const auto pipeline = compiler::parse_pipeline_toml(
      pipeline_text, "fixtures/valid_pipeline.toml");
  const auto reordered_schema = compiler::parse_schema_toml(
      read_text(fixtures / "valid_schema_reordered.toml"),
      "other/valid_schema.toml");
  const auto reordered_pipeline = compiler::parse_pipeline_toml(
      read_text(fixtures / "valid_pipeline_reordered.toml"),
      "other/valid_pipeline.toml");
  const auto unknown_key_fixture = compiler::parse_schema_toml(
      read_text(fixtures / "invalid_schema_unknown_key.toml"),
      "fixtures/invalid_schema_unknown_key.toml");
  expect_parse_code(unknown_key_fixture, "FFSCHEMA002",
                    "unknown-key TOML fixture is rejected");
  check(schema.has_value(), "valid schema fixture parses");
  check(pipeline.has_value(), "valid pipeline fixture parses");
  check(reordered_schema.has_value(), "reordered schema fixture parses");
  check(reordered_pipeline.has_value(), "reordered pipeline fixture parses");

  test_names();
  test_sha256_and_json();
  test_structural_parsing(schema_text, pipeline_text);
  test_diagnostic_format();
  if (schema && pipeline && reordered_schema && reordered_pipeline) {
    const auto resolved_schema = compiler::validate_schema(*schema);
    check(resolved_schema.has_value(), "valid schema fixture validates");
    if (resolved_schema) {
      test_schema_validation(*schema);
      test_pipeline_validation(*pipeline, *resolved_schema);
      const auto wildcard_fixture = compiler::parse_pipeline_toml(
          read_text(fixtures / "invalid_pipeline_wildcard.toml"),
          "fixtures/invalid_pipeline_wildcard.toml");
      check(wildcard_fixture.has_value(),
            "invalid wildcard fixture parses before semantic validation");
      if (wildcard_fixture) {
        const auto wildcard_result =
            compiler::validate_pipeline(*wildcard_fixture, *resolved_schema);
        check(!wildcard_result && wildcard_result.error().code == "FFPIPE016",
              "invalid wildcard fixture has exact diagnostic code");
      }
    }
    test_lowering(*schema, *pipeline, *reordered_schema, *reordered_pipeline);
  }

  if (failures != 0) {
    std::cerr << failures << " compiler frontend test(s) failed\n";
    return 1;
  }
  return 0;
}
