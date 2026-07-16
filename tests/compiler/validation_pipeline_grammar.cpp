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
      compiler::parse_pipeline_toml(text, "audit\\pipeline.toml"), code,
      object, message, description,
      std::string_view{"audit/pipeline.toml"});
}

void test_closed_grammar(support::suite& tests) {
  const std::string base{support::valid_pipeline_toml};

  expect_parse_error(
      tests,
      support::replace_once(tests, base, "[[emit]]",
                            "unknown_top = true\n\n[[emit]]",
                            "pipeline top unknown key"),
      "FFPIPE002", "pipeline.unknown_top", "unknown key 'unknown_top'",
      "unknown top-level pipeline key");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "fields = [\"payload\"]",
                            "fields = [\"payload\"]\nunknown_emit = true",
                            "emit unknown key"),
      "FFPIPE002", "pipeline.emit[0].unknown_emit",
      "unknown key 'unknown_emit'", "unknown emit-table key");
}

void test_required_keys(support::suite& tests) {
  const std::string base{support::valid_pipeline_toml};
  struct required_case {
    std::string_view line;
    std::string_view code;
    std::string_view object;
    std::string_view key;
  };
  constexpr std::array top_cases{
      required_case{"format_version = 1\n", "FFPIPE001",
                    "pipeline.format_version", "format_version"},
      required_case{"name = \"test_pipeline\"\n", "FFPIPE003",
                    "pipeline.name", "name"},
      required_case{
          "namespace = \"FeedForge::generated::test_pipeline\"\n",
          "FFPIPE003", "pipeline.namespace", "namespace"},
      required_case{"schema = \"test_feed\"\n", "FFPIPE003",
                    "pipeline.schema", "schema"},
      required_case{"profile = \"portable_checked\"\n", "FFPIPE003",
                    "pipeline.profile", "profile"},
      required_case{"unknown_messages = \"error\"\n", "FFPIPE003",
                    "pipeline.unknown_messages", "unknown_messages"},
      required_case{"unselected_messages = \"skip\"\n", "FFPIPE003",
                    "pipeline.unselected_messages", "unselected_messages"},
  };
  for (const required_case& item : top_cases) {
    const std::string description =
        "pipeline missing key " + std::string(item.key);
    expect_parse_error(
        tests, support::erase_once(tests, base, item.line, description),
        item.code, item.object,
        "missing required key '" + std::string(item.key) + "'", description);
  }

  std::string without_emit = base;
  const std::size_t emit = without_emit.find("[[emit]]");
  tests.check(emit != std::string::npos,
              "emit mutation source exists");
  if (emit != std::string::npos) {
    without_emit.erase(emit);
  }
  expect_parse_error(tests, without_emit, "FFPIPE003", "pipeline.emit",
                     "missing required key 'emit'",
                     "pipeline missing emit array");

  constexpr std::array emit_cases{
      required_case{"source = \"A\"\n", "FFPIPE003",
                    "pipeline.emit[0].source", "source"},
      required_case{"event = \"event\"\n", "FFPIPE003",
                    "pipeline.emit[0].event", "event"},
      required_case{"fields = [\"payload\"]\n", "FFPIPE003",
                    "pipeline.emit[0].fields", "fields"},
  };
  for (const required_case& item : emit_cases) {
    const std::string description =
        "emit missing key " + std::string(item.key);
    expect_parse_error(
        tests, support::erase_once(tests, base, item.line, description),
        item.code, item.object,
        "missing required key '" + std::string(item.key) + "'", description);
  }
}

void test_toml_types_and_duplicates(support::suite& tests) {
  const std::string base{support::valid_pipeline_toml};

  expect_parse_error(
      tests,
      support::replace_once(tests, base, "format_version = 1",
                            "format_version = \"one\"",
                            "pipeline version TOML type"),
      "FFPIPE004", "pipeline.format_version", "must be an integer",
      "pipeline integer key with string value");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "name = \"test_pipeline\"",
                            "name = 1", "pipeline name TOML type"),
      "FFPIPE004", "pipeline.name", "must be a string",
      "pipeline string key with integer value");
  struct string_case {
    std::string_view text;
    std::string_view replacement;
    std::string_view object;
    std::string_view description;
  };
  constexpr std::array string_cases{
      string_case{"namespace = \"FeedForge::generated::test_pipeline\"",
                  "namespace = 1", "pipeline.namespace",
                  "pipeline namespace has wrong TOML type"},
      string_case{"schema = \"test_feed\"", "schema = 1",
                  "pipeline.schema",
                  "pipeline schema has wrong TOML type"},
      string_case{"profile = \"portable_checked\"", "profile = 1",
                  "pipeline.profile",
                  "pipeline profile has wrong TOML type"},
      string_case{"unknown_messages = \"error\"", "unknown_messages = 1",
                  "pipeline.unknown_messages",
                  "unknown-message policy has wrong TOML type"},
      string_case{"unselected_messages = \"skip\"",
                  "unselected_messages = 1",
                  "pipeline.unselected_messages",
                  "unselected-message policy has wrong TOML type"},
      string_case{"event = \"event\"", "event = 1",
                  "pipeline.emit[0].event",
                  "emit event has wrong TOML type"},
  };
  for (const string_case& item : string_cases) {
    expect_parse_error(
        tests,
        support::replace_once(tests, base, item.text, item.replacement,
                              item.description),
        "FFPIPE004", item.object, "must be a string", item.description);
  }
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "source = \"A\"", "source = 65",
                            "emit source TOML type"),
      "FFPIPE004", "pipeline.emit[0].source", "must be a string",
      "emit source with integer value");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "fields = [\"payload\"]",
                            "fields = \"payload\"",
                            "emit fields TOML type"),
      "FFPIPE004", "pipeline.emit[0].fields", "must be an array",
      "emit fields with scalar value");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "fields = [\"payload\"]",
                            "fields = [\"payload\", 1]",
                            "emit field item TOML type"),
      "FFPIPE004", "pipeline.emit.event.fields",
      "fields must contain only strings",
      "emit fields with non-string item");

  std::string non_table = base;
  const std::size_t emit = non_table.find("[[emit]]");
  tests.check(emit != std::string::npos,
              "non-table emit mutation source exists");
  if (emit != std::string::npos) {
    std::string scalar_emit = base;
    scalar_emit.replace(emit, std::string::npos, "emit = \"wrong\"\n");
    expect_parse_error(tests, scalar_emit, "FFPIPE004", "pipeline.emit",
                       "emit must be an array of tables",
                       "emit value is not an array");
    non_table.replace(emit, std::string::npos, "emit = [\"not a table\"]\n");
  }
  expect_parse_error(tests, non_table, "FFPIPE004", "pipeline.emit[0]",
                     "each emit entry must be a table",
                     "emit array with non-table entry");

  expect_parse_error(
      tests,
      support::replace_once(
          tests, base, "name = \"test_pipeline\"",
          "name = \"test_pipeline\"\nname = \"duplicate\"",
          "duplicate pipeline name key"),
      "FFTOML001", "pipeline", "name",
      "duplicate pipeline TOML name key");
  expect_parse_error(
      tests,
      support::replace_once(tests, base, "event = \"event\"",
                            "event = \"event\"\nevent = \"duplicate\"",
                            "duplicate emit event key"),
      "FFTOML001", "pipeline", "event",
      "duplicate emit TOML key");
}

void test_positive_grammar(support::suite& tests) {
  const auto schema_source = compiler::parse_schema_toml(
      support::valid_schema_toml, "audit/schema.toml");
  tests.check(schema_source.has_value(),
              "pipeline grammar schema parses");
  if (!schema_source) {
    return;
  }
  const auto schema = compiler::validate_schema(*schema_source);
  tests.check(schema.has_value(),
              "pipeline grammar schema validates");
  if (!schema) {
    return;
  }

  const auto parsed = compiler::parse_pipeline_toml(
      support::valid_pipeline_toml, "audit/pipeline.toml");
  tests.check(parsed.has_value(),
              "complete pipeline grammar fixture parses");
  if (parsed) {
    tests.check(compiler::validate_pipeline(*parsed, *schema).has_value(),
                "complete pipeline grammar fixture validates");
  }

  constexpr std::string_view reordered = R"(# Key and table ordering is non-semantic.
unselected_messages = "skip"
unknown_messages = "skip"
profile = "portable_checked"
schema = "test_feed"
namespace = "FeedForge::generated::test_pipeline"
name = "test_pipeline"
format_version = 1

[[emit]]
fields = ["payload"]
event = "event"
source = "A"
)";
  const auto reordered_result =
      compiler::parse_pipeline_toml(reordered, "audit/reordered.toml");
  tests.check(reordered_result.has_value(),
              "comments and reordered pipeline keys parse");
  if (reordered_result) {
    const auto resolved =
        compiler::validate_pipeline(*reordered_result, *schema);
    tests.check(resolved.has_value(),
                "comments and reordered pipeline keys validate");
    if (resolved) {
      tests.check(resolved->unknown_messages == "skip",
                  "accepted unknown-message skip policy is preserved");
    }
  }
}

}  // namespace

int main() {
  support::suite tests;
  test_closed_grammar(tests);
  test_required_keys(tests);
  test_toml_types_and_duplicates(tests);
  test_positive_grammar(tests);
  return tests.finish("pipeline grammar");
}
