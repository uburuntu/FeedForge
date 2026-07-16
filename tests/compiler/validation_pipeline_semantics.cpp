#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "validation_test_support.hpp"

namespace {

namespace compiler = feedforge::compiler;
namespace support = feedforge::compiler::test;

[[nodiscard]] compiler::schema_source two_message_schema() {
  compiler::schema_source source = support::minimal_schema();
  compiler::message_source second = source.messages.front();
  second.name = "other_event";
  second.type = "B";
  second.fields[0].value = "B";
  source.messages.push_back(std::move(second));
  return source;
}

void test_pipeline_metadata(support::suite& tests,
                            const compiler::resolved_schema& schema) {
  const compiler::pipeline_source base = support::minimal_pipeline();

  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.format_version = 2; }, "FFPIPE001",
      "pipeline.format_version", "must be exactly 1",
      "unsupported pipeline format version");
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.schema = "Other"; }, "FFPIPE005",
      "pipeline.schema", "schema reference must match",
      "invalid schema reference identifier");
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.schema = "other_schema"; }, "FFPIPE007",
      "pipeline.schema", "does not match parsed schema 'test_feed'",
      "pipeline schema mismatch");
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.profile = "portable_fast"; }, "FFPIPE008",
      "pipeline.profile", "unsupported profile 'portable_fast'",
      "unsupported implementation profile");
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.unknown_messages = "ignore"; }, "FFPIPE009",
      "pipeline.unknown_messages",
      "unknown_messages must be \"error\" or \"skip\"",
      "unsupported unknown-message policy");
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.unselected_messages = "error"; },
      "FFPIPE009", "pipeline.unselected_messages",
      "unselected_messages must be \"skip\"",
      "unsupported unselected-message policy");
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.projections.clear(); }, "FFPIPE010",
      "pipeline.emit", "must define at least one emit",
      "pipeline with no emits");

  for (const std::string_view policy : {"error", "skip"}) {
    compiler::pipeline_source candidate = base;
    candidate.unknown_messages = policy;
    const auto resolved = compiler::validate_pipeline(candidate, schema);
    tests.check(resolved.has_value(),
                std::string("accepted unknown-message policy ") +
                    std::string(policy));
    if (resolved) {
      tests.check(resolved->unknown_messages == policy &&
                      resolved->unselected_messages == "skip" &&
                      resolved->profile == "portable_checked",
                  std::string("profile and policy preserved for ") +
                      std::string(policy));
    }
  }
}

void test_emit_source_and_uniqueness(support::suite& tests,
                                     const compiler::resolved_schema& schema,
                                     const compiler::resolved_schema&
                                         two_messages) {
  const compiler::pipeline_source base = support::minimal_pipeline();

  for (const std::string& source :
       {std::string{}, std::string{"AA"}, std::string{"\xC3\xA9"}}) {
    support::expect_pipeline_error(
        tests, base, schema,
        [&source](auto& pipeline) {
          pipeline.projections[0].source = source;
        },
        "FFPIPE011", "pipeline.emit.event.source",
        "source must be exactly one byte",
        "emit source is not exactly one byte");
  }
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.projections[0].source = "Z"; },
      "FFPIPE012", "pipeline.emit.event.source",
      "unknown source message 'Z'", "unknown source message");

  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) {
        compiler::projection_source duplicate = source.projections.front();
        duplicate.event = "other_event";
        source.projections.push_back(std::move(duplicate));
      },
      "FFPIPE013", "pipeline.emit.other_event.source",
      "duplicate source selection 'A'", "duplicate source selection");

  support::expect_pipeline_error(
      tests, base, two_messages,
      [](auto& source) {
        compiler::projection_source duplicate = source.projections.front();
        duplicate.source = "B";
        source.projections.push_back(std::move(duplicate));
      },
      "FFPIPE014", "pipeline.emit.event.event",
      "duplicate event name 'event'", "duplicate semantic event name");

  compiler::pipeline_source ordered = base;
  compiler::projection_source first = ordered.projections.front();
  first.source = "B";
  first.event = "other_event";
  ordered.projections.insert(ordered.projections.begin(), std::move(first));
  const auto resolved = compiler::validate_pipeline(ordered, two_messages);
  tests.check(resolved.has_value(),
              "two unique source/event selections validate");
  if (resolved) {
    tests.check(resolved->projections.size() == 2U &&
                    resolved->projections[0].source_discriminator <
                        resolved->projections[1].source_discriminator,
                "emit table order canonicalizes by unsigned discriminator");
  }

  compiler::schema_source case_source = support::minimal_schema();
  compiler::message_source lowercase = case_source.messages.front();
  lowercase.name = "lowercase_event";
  lowercase.type = "a";
  lowercase.fields[0].value = "a";
  case_source.messages.push_back(std::move(lowercase));
  const auto case_schema = compiler::validate_schema(case_source);
  tests.check(case_schema.has_value(),
              "case-sensitive source schema validates");
  if (case_schema) {
    compiler::pipeline_source case_pipeline = base;
    compiler::projection_source lowercase_projection =
        case_pipeline.projections.front();
    lowercase_projection.source = "a";
    lowercase_projection.event = "lowercase_event";
    case_pipeline.projections.push_back(std::move(lowercase_projection));
    tests.check(
        compiler::validate_pipeline(case_pipeline, *case_schema).has_value(),
        "uppercase and lowercase one-byte sources select distinct messages");
  }
}

void test_field_selection_rules(support::suite& tests,
                                const compiler::resolved_schema& schema) {
  const compiler::pipeline_source base = support::minimal_pipeline();

  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) { source.projections[0].fields.clear(); },
      "FFPIPE015", "pipeline.emit.event.fields",
      "emit fields must not be empty", "emit with empty field list");
  for (const std::array<std::string, 2>& fields :
       {std::array<std::string, 2>{"*", "payload"},
        std::array<std::string, 2>{"payload", "*"},
        std::array<std::string, 2>{"*", "*"}}) {
    support::expect_pipeline_error(
        tests, base, schema,
        [&fields](auto& source) {
          source.projections[0].fields.assign(fields.begin(), fields.end());
        },
        "FFPIPE016", "pipeline.emit.event.fields",
        "wildcard must appear alone",
        "mixed or repeated wildcard field selection");
  }
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) {
        source.projections[0].fields = {"payload", "payload"};
      },
      "FFPIPE017", "pipeline.emit.event.fields",
      "duplicate projected field 'payload'",
      "duplicate explicit field selection");
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) {
        source.projections[0].fields = {"missing"};
      },
      "FFPIPE018", "pipeline.emit.event.fields",
      "has no field named 'missing'", "unknown explicit field");
  support::expect_pipeline_error(
      tests, base, schema,
      [](auto& source) {
        source.projections[0].fields = {"message_type"};
      },
      "FFPIPE019", "pipeline.emit.event.fields",
      "is a discriminator or reserved field",
      "explicit discriminator projection");

  compiler::schema_source reserved_source = support::minimal_schema();
  reserved_source.messages[0].fields[1].type = "reserved";
  const auto reserved_schema = compiler::validate_schema(reserved_source);
  tests.check(reserved_schema.has_value(),
              "reserved projection test schema validates");
  if (reserved_schema) {
    support::expect_pipeline_error(
        tests, base, *reserved_schema, [](auto&) {}, "FFPIPE019",
        "pipeline.emit.event.fields",
        "is a discriminator or reserved field",
        "explicit reserved-field projection");
    support::expect_pipeline_error(
        tests, base, *reserved_schema,
        [](auto& source) { source.projections[0].fields = {"*"}; },
        "FFPIPE015", "pipeline.emit.event.fields",
        "resolves to no projectable fields",
        "wildcard with no projectable fields");
  }

  compiler::resolved_schema no_representation = schema;
  no_representation.messages[0].fields[1].logical =
      compiler::logical_kind::timestamp_ns;
  support::expect_pipeline_error(
      tests, base, no_representation, [](auto&) {}, "FFPIPE020",
      "pipeline.emit.event.fields",
      "has no C++20 value representation",
      "projected field without C++20 representation");
}

void test_wildcard_and_explicit_acceptance(support::suite& tests) {
  compiler::schema_source source = support::minimal_schema();
  source.messages[0].size = 4;
  source.messages[0].fields.push_back(compiler::field_source{
      .mark = support::test_mark(),
      .name = "padding",
      .type = "reserved",
      .offset = 2,
      .width = 1,
      .role = std::nullopt,
      .value = std::nullopt,
      .allowed = {},
  });
  source.messages[0].fields.push_back(compiler::field_source{
      .mark = support::test_mark(),
      .name = "later",
      .type = "alpha",
      .offset = 3,
      .width = 1,
      .role = std::nullopt,
      .value = std::nullopt,
      .allowed = {},
  });
  std::ranges::rotate(source.messages[0].fields,
                      source.messages[0].fields.begin() + 1);
  const auto schema = compiler::validate_schema(source);
  tests.check(schema.has_value(),
              "wildcard ordering schema validates");
  if (!schema) {
    return;
  }

  compiler::pipeline_source wildcard = support::minimal_pipeline();
  wildcard.projections[0].fields = {"*"};
  const auto wildcard_result =
      compiler::validate_pipeline(wildcard, *schema);
  tests.check(wildcard_result.has_value(),
              "exact standalone wildcard validates");
  if (wildcard_result) {
    const auto& fields = wildcard_result->projections.front().fields;
    tests.check(fields.size() == 2U && fields[0].name == "payload" &&
                    fields[1].name == "later",
                "wildcard expands projectable fields in wire order and "
                "excludes discriminator/reserved fields");
  }

  compiler::pipeline_source explicit_order = support::minimal_pipeline();
  explicit_order.projections[0].fields = {"later", "payload"};
  const auto explicit_result =
      compiler::validate_pipeline(explicit_order, *schema);
  tests.check(explicit_result.has_value(),
              "unique nonempty explicit field list validates");
  if (explicit_result) {
    const auto& fields = explicit_result->projections.front().fields;
    tests.check(fields.size() == 2U && fields[0].name == "later" &&
                    fields[1].name == "payload",
                "explicit fields preserve listed semantic order");
  }
}

void test_generated_scope_collisions(support::suite& tests,
                                     const compiler::resolved_schema& schema) {
  const compiler::pipeline_source base = support::minimal_pipeline();
  for (const std::string_view declaration :
       {"basic_decoder", "decoder", "pipeline_metadata",
        "sink_for_all_selected_events"}) {
    support::expect_pipeline_error(
        tests, base, schema,
        [declaration](auto& source) {
          source.projections[0].event = declaration;
        },
        "FFPIPE021", ".event",
        "collides with a generated pipeline declaration",
        std::string("event collides with generated declaration ") +
            std::string(declaration));
  }

  for (const std::string_view field_name :
       {"source_discriminator", "event_name", "event"}) {
    compiler::resolved_schema collision_schema = schema;
    collision_schema.messages[0].fields[1].name = field_name;
    compiler::pipeline_source projection = base;
    projection.projections[0].fields = {std::string(field_name)};
    tests.expect_error(
        compiler::validate_pipeline(projection, collision_schema),
        "FFPIPE021", "pipeline.emit.event.fields",
        "collides with a generated event declaration",
        std::string("field collides with generated declaration ") +
            std::string(field_name),
        std::string_view{"fixtures/pipeline.toml"});
  }
}

}  // namespace

int main() {
  support::suite tests;
  const auto schema =
      compiler::validate_schema(support::minimal_schema());
  const auto two_messages =
      compiler::validate_schema(two_message_schema());
  tests.check(schema.has_value(),
              "pipeline semantic schema validates");
  tests.check(two_messages.has_value(),
              "two-message pipeline semantic schema validates");
  if (!schema || !two_messages) {
    return tests.finish("pipeline semantic setup");
  }

  test_pipeline_metadata(tests, *schema);
  test_emit_source_and_uniqueness(tests, *schema, *two_messages);
  test_field_selection_rules(tests, *schema);
  test_wildcard_and_explicit_acceptance(tests);
  test_generated_scope_collisions(tests, *schema);
  return tests.finish("pipeline semantic");
}
