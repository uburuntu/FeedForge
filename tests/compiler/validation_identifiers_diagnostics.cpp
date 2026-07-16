#include <array>
#include <string>
#include <string_view>

#include "validation_test_support.hpp"

namespace {

namespace compiler = feedforge::compiler;
namespace support = feedforge::compiler::test;

constexpr std::array cpp_keywords{
    std::string_view{"alignas"},     std::string_view{"alignof"},
    std::string_view{"and"},         std::string_view{"and_eq"},
    std::string_view{"asm"},         std::string_view{"auto"},
    std::string_view{"bitand"},      std::string_view{"bitor"},
    std::string_view{"bool"},        std::string_view{"break"},
    std::string_view{"case"},        std::string_view{"catch"},
    std::string_view{"char"},        std::string_view{"char16_t"},
    std::string_view{"char32_t"},    std::string_view{"char8_t"},
    std::string_view{"class"},       std::string_view{"co_await"},
    std::string_view{"co_return"},   std::string_view{"co_yield"},
    std::string_view{"compl"},       std::string_view{"concept"},
    std::string_view{"const"},       std::string_view{"const_cast"},
    std::string_view{"consteval"},   std::string_view{"constexpr"},
    std::string_view{"constinit"},   std::string_view{"continue"},
    std::string_view{"decltype"},    std::string_view{"default"},
    std::string_view{"delete"},      std::string_view{"do"},
    std::string_view{"double"},      std::string_view{"dynamic_cast"},
    std::string_view{"else"},        std::string_view{"enum"},
    std::string_view{"explicit"},    std::string_view{"export"},
    std::string_view{"extern"},      std::string_view{"false"},
    std::string_view{"float"},       std::string_view{"for"},
    std::string_view{"friend"},      std::string_view{"goto"},
    std::string_view{"if"},          std::string_view{"inline"},
    std::string_view{"int"},         std::string_view{"long"},
    std::string_view{"mutable"},     std::string_view{"namespace"},
    std::string_view{"new"},         std::string_view{"noexcept"},
    std::string_view{"not"},         std::string_view{"not_eq"},
    std::string_view{"nullptr"},     std::string_view{"operator"},
    std::string_view{"or"},          std::string_view{"or_eq"},
    std::string_view{"private"},     std::string_view{"protected"},
    std::string_view{"public"},      std::string_view{"register"},
    std::string_view{"reinterpret_cast"},
    std::string_view{"requires"},    std::string_view{"return"},
    std::string_view{"short"},       std::string_view{"signed"},
    std::string_view{"sizeof"},      std::string_view{"static"},
    std::string_view{"static_assert"},
    std::string_view{"static_cast"}, std::string_view{"struct"},
    std::string_view{"switch"},      std::string_view{"template"},
    std::string_view{"this"},        std::string_view{"thread_local"},
    std::string_view{"throw"},       std::string_view{"true"},
    std::string_view{"try"},         std::string_view{"typedef"},
    std::string_view{"typeid"},      std::string_view{"typename"},
    std::string_view{"union"},       std::string_view{"unsigned"},
    std::string_view{"using"},       std::string_view{"virtual"},
    std::string_view{"void"},        std::string_view{"volatile"},
    std::string_view{"wchar_t"},     std::string_view{"while"},
    std::string_view{"xor"},         std::string_view{"xor_eq"},
};
static_assert(cpp_keywords.size() == 92U);

void test_lexical_forms(support::suite& tests) {
  for (const std::string_view accepted :
       {"a", "a0", "a_b", "z9_", "portable_checked"}) {
    tests.check(compiler::is_valid_source_name(accepted),
                std::string("accepted source identifier ") +
                    std::string(accepted));
  }
  for (const std::string_view rejected :
       {"", "Upper", "_hidden", "1name", "bad-name", "bad.name",
        "bad__name", "na\xC3\xAFve"}) {
    tests.check(!compiler::is_valid_source_name(rejected),
                std::string("rejected source identifier form ") +
                    std::string(rejected));
  }

  for (const std::string_view accepted :
       {"a", "A", "FeedForge", "a_1", "Z9_"}) {
    tests.check(compiler::is_valid_namespace_component(accepted),
                std::string("accepted namespace component ") +
                    std::string(accepted));
  }
  for (const std::string_view rejected :
       {"", "_hidden", "__hidden", "1name", "bad-name", "bad.name",
        "bad__name", "na\xC3\xAFve"}) {
    tests.check(!compiler::is_valid_namespace_component(rejected),
                std::string("rejected namespace component form ") +
                    std::string(rejected));
  }

  for (const std::string_view keyword : cpp_keywords) {
    tests.check(compiler::is_cpp_keyword(keyword),
                std::string("recognized C++ keyword ") +
                    std::string(keyword));
    tests.check(!compiler::is_valid_source_name(keyword),
                std::string("source name rejects C++ keyword ") +
                    std::string(keyword));
    tests.check(!compiler::is_valid_namespace_component(keyword),
                std::string("namespace rejects C++ keyword ") +
                    std::string(keyword));
  }
}

void test_all_source_name_scopes(support::suite& tests) {
  const compiler::schema_source schema = support::minimal_schema();
  const auto resolved = compiler::validate_schema(schema);
  tests.check(resolved.has_value(),
              "identifier test schema resolves");
  if (!resolved) {
    return;
  }
  const compiler::pipeline_source pipeline = support::minimal_pipeline();

  for (const std::string_view invalid :
       {"", "Upper", "_hidden", "1name", "bad-name", "bad__name", "class",
        "na\xC3\xAFve"}) {
    const std::string label =
        "invalid identifier form '" + std::string(invalid) + "'";
    support::expect_schema_error(
        tests, schema,
        [invalid](auto& source) { source.name = invalid; }, "FFSCHEMA005",
        "schema.name", "schema name must match", label + " as schema name");
    support::expect_schema_error(
        tests, schema,
        [invalid](auto& source) {
          source.types.push_back(compiler::type_source{
              .mark = support::test_mark(),
              .name = std::string(invalid),
              .kind = "uint",
              .width = 1,
              .logical = "raw_unsigned",
              .scale = std::nullopt,
          });
        },
        "FFSCHEMA005", ".name", "type name must match",
        label + " as type name");
    support::expect_schema_error(
        tests, schema,
        [invalid](auto& source) {
          source.messages[0].name = invalid;
        },
        "FFSCHEMA005", ".name", "message name must match",
        label + " as message name");
    support::expect_schema_error(
        tests, schema,
        [invalid](auto& source) {
          source.messages[0].fields[1].name = invalid;
        },
        "FFSCHEMA005", ".name", "field name must match",
        label + " as field name");
    support::expect_pipeline_error(
        tests, pipeline, *resolved,
        [invalid](auto& source) { source.name = invalid; }, "FFPIPE005",
        "pipeline.name", "pipeline name must match",
        label + " as pipeline name");
    support::expect_pipeline_error(
        tests, pipeline, *resolved,
        [invalid](auto& source) {
          source.projections[0].event = invalid;
        },
        "FFPIPE005", ".event", "event name must match",
        label + " as event name");
    support::expect_pipeline_error(
        tests, pipeline, *resolved,
        [invalid](auto& source) { source.profile = invalid; }, "FFPIPE008",
        "pipeline.profile", "unsupported profile",
        label + " as profile name");
  }
}

void test_reserved_type_names(support::suite& tests) {
  const compiler::schema_source base = support::minimal_schema();
  for (const std::string_view builtin :
       {"u8", "u16", "u32", "u48", "u64", "alpha", "reserved"}) {
    support::expect_schema_error(
        tests, base,
        [builtin](auto& source) {
          source.types.push_back(compiler::type_source{
              .mark = support::test_mark(),
              .name = std::string(builtin),
              .kind = "uint",
              .width = 1,
              .logical = "raw_unsigned",
              .scale = std::nullopt,
          });
        },
        "FFSCHEMA007", ".name", "shadows a reserved built-in type",
        std::string("user type shadows built-in ") + std::string(builtin));
  }
}

void test_namespace_forms(support::suite& tests) {
  const auto schema = compiler::validate_schema(support::minimal_schema());
  tests.check(schema.has_value(),
              "namespace test schema resolves");
  if (!schema) {
    return;
  }
  const compiler::pipeline_source base = support::minimal_pipeline();
  for (const std::string_view invalid :
       {"", "::name", "name::", "name:::other", "_hidden", "name::_hidden",
        "name::bad__part", "name::class", "name::1part", "name::bad-part",
        "name::na\xC3\xAFve"}) {
    support::expect_pipeline_error(
        tests, base, *schema,
        [invalid](auto& source) { source.cpp_namespace = invalid; },
        "FFPIPE006", "pipeline.namespace",
        invalid.empty() ? "at least one component"
                        : "invalid or reserved C++ namespace component",
        std::string("invalid namespace form '") + std::string(invalid) + "'");
  }
}

void test_exact_name_preservation(support::suite& tests) {
  compiler::schema_source schema_source = support::minimal_schema();
  schema_source.name = "schema_9";
  schema_source.messages[0].name = "event_8";
  schema_source.messages[0].fields[1].name = "payload_7";
  const auto schema = compiler::validate_schema(schema_source);
  tests.check(schema.has_value(),
              "accepted schema identifiers validate without normalization");
  if (!schema) {
    return;
  }
  tests.check(schema->name == "schema_9" &&
                  schema->messages.front().name == "event_8" &&
                  schema->messages.front().fields[1].name == "payload_7",
              "schema identifiers preserve exact accepted spelling");

  compiler::pipeline_source pipeline_source = support::minimal_pipeline();
  pipeline_source.name = "pipeline_6";
  pipeline_source.cpp_namespace = "FeedForge::Generated_5";
  pipeline_source.schema = "schema_9";
  pipeline_source.projections[0].event = "output_4";
  pipeline_source.projections[0].fields = {"payload_7"};
  const auto pipeline =
      compiler::validate_pipeline(pipeline_source, *schema);
  tests.check(pipeline.has_value(),
              "accepted pipeline identifiers validate without normalization");
  if (pipeline) {
    tests.check(pipeline->name == "pipeline_6" &&
                    pipeline->cpp_namespace == "FeedForge::Generated_5" &&
                    pipeline->projections.front().event == "output_4" &&
                    pipeline->profile == "portable_checked",
                "pipeline, namespace, event, and profile spellings are exact");
  }
}

void test_source_path_diagnostics(support::suite& tests) {
  tests.check(
      compiler::normalise_source_path("relative\\nested\\schema.toml") ==
          "relative/nested/schema.toml",
      "diagnostic path normalizes only separators");
  tests.check(compiler::normalise_source_path("relative/../schema.toml") ==
                  "relative/../schema.toml",
              "diagnostic path does not canonicalize dot-dot components");

  const auto syntax = compiler::parse_schema_toml(
      "format_version = [", "relative\\nested\\schema.toml");
  tests.expect_error(syntax, "FFTOML001", "schema", "", "malformed TOML",
                     std::string_view{"relative/nested/schema.toml"});
  if (!syntax) {
    tests.check(syntax.error().position.has_value(),
                "TOML syntax diagnostic includes source line and column");
  }

  const std::string unknown = support::replace_once(
      tests, std::string{support::valid_schema_toml}, "[[types]]",
      "typo = true\n\n[[types]]", "path diagnostic unknown key");
  const auto structural =
      compiler::parse_schema_toml(unknown, "relative/../schema.toml");
  tests.expect_error(structural, "FFSCHEMA002", "schema.typo",
                     "unknown key 'typo'", "structural schema diagnostic",
                     std::string_view{"relative/../schema.toml"});
  if (!structural) {
    tests.check(structural.error().position.has_value(),
                "structural schema diagnostic includes source position");
  }

  auto parsed = compiler::parse_schema_toml(
      support::valid_schema_toml, "relative\\semantic\\schema.toml");
  tests.check(parsed.has_value(),
              "semantic path fixture parses");
  if (parsed) {
    parsed->name = "class";
    tests.expect_error(
        compiler::validate_schema(*parsed), "FFSCHEMA005", "schema.name",
        "schema name must match", "semantic schema diagnostic",
        std::string_view{"relative/semantic/schema.toml"});
  }

  const std::string pipeline_unknown = support::replace_once(
      tests, std::string{support::valid_pipeline_toml}, "[[emit]]",
      "typo = true\n\n[[emit]]", "pipeline path diagnostic unknown key");
  tests.expect_error(
      compiler::parse_pipeline_toml(pipeline_unknown,
                                    "relative\\pipeline.toml"),
      "FFPIPE002", "pipeline.typo", "unknown key 'typo'",
      "structural pipeline diagnostic",
      std::string_view{"relative/pipeline.toml"});

  tests.expect_error(
      compiler::parse_schema_file(
          "definitely-missing-ff800\\schema.toml"),
      "FFIO001", "schema", "not a readable regular file",
      "missing source file diagnostic",
      std::string_view{"definitely-missing-ff800/schema.toml"});

  const compiler::diagnostic problem = compiler::make_diagnostic(
      "FFTEST001", "fixtures\\schema.toml",
      support::test_mark(12U, 7U), "schema.messages.event",
      "example problem", "fix the fixture");
  tests.check(
      compiler::format_diagnostic(problem) ==
          "FFTEST001 fixtures/schema.toml:12:7: "
          "schema.messages.event: example problem\nhint: fix the fixture",
      "formatted diagnostic source, object, text, and hint are stable");
}

}  // namespace

int main() {
  support::suite tests;
  test_lexical_forms(tests);
  test_all_source_name_scopes(tests);
  test_reserved_type_names(tests);
  test_namespace_forms(tests);
  test_exact_name_preservation(tests);
  test_source_path_diagnostics(tests);
  return tests.finish("identifier and diagnostic");
}
