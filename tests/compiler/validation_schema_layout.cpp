#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "validation_test_support.hpp"

namespace {

namespace compiler = feedforge::compiler;
namespace support = feedforge::compiler::test;

void test_message_type_and_size_bounds(support::suite& tests) {
  const compiler::schema_source base = support::minimal_schema();

  compiler::schema_source minimum = base;
  minimum.messages[0].size = 1;
  minimum.messages[0].fields.pop_back();
  tests.check(compiler::validate_schema(minimum).has_value(),
              "message size lower bound 1 validates");

  compiler::schema_source maximum = base;
  maximum.messages[0].size = 65535;
  maximum.messages[0].fields[1].type = "reserved";
  maximum.messages[0].fields[1].width = 65534;
  tests.check(compiler::validate_schema(maximum).has_value(),
              "message size upper bound 65535 validates");

  for (const std::int64_t size :
       {std::int64_t{0}, std::int64_t{-1}, std::int64_t{65536}}) {
    support::expect_schema_error(
        tests, base,
        [size](auto& source) { source.messages[0].size = size; },
        "FFSCHEMA018", "schema.messages.event.size",
        "message size must be in [1, 65535]",
        std::string("message size rejected: ") + std::to_string(size));
  }

  for (const std::string_view discriminator : {"!", "~"}) {
    compiler::schema_source boundary = base;
    boundary.messages[0].type = discriminator;
    boundary.messages[0].fields[0].value = std::string(discriminator);
    tests.check(compiler::validate_schema(boundary).has_value(),
                std::string("printable discriminator boundary ") +
                    std::string(discriminator) + " validates");
  }

  const std::array invalid_discriminators{
      std::string{}, std::string{"AA"}, std::string{" "},
      std::string{static_cast<char>(0x7f)},
      std::string{static_cast<char>(0x80)}};
  for (const std::string& discriminator : invalid_discriminators) {
    support::expect_schema_error(
        tests, base,
        [&discriminator](auto& source) {
          source.messages[0].type = discriminator;
        },
        "FFSCHEMA017", "schema.messages.event.type",
        "exactly one printable ASCII byte",
        "invalid message discriminator byte sequence");
  }
}

void test_duplicate_names_and_discriminators(support::suite& tests) {
  const compiler::schema_source base = support::minimal_schema();

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        const compiler::type_source type{
            .mark = support::test_mark(),
            .name = "semantic",
            .kind = "uint",
            .width = 1,
            .logical = "raw_unsigned",
            .scale = std::nullopt,
        };
        source.types = {type, type};
      },
      "FFSCHEMA006", "schema.types.semantic.name",
      "duplicate type name 'semantic'", "duplicate semantic type name");

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        compiler::message_source duplicate = source.messages.front();
        duplicate.type = "B";
        duplicate.fields[0].value = "B";
        source.messages.push_back(std::move(duplicate));
      },
      "FFSCHEMA008", "schema.messages.event.name",
      "duplicate message name 'event'", "duplicate message name");

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        compiler::message_source duplicate = source.messages.front();
        duplicate.name = "other_event";
        source.messages.push_back(std::move(duplicate));
      },
      "FFSCHEMA009", "schema.messages.other_event.type",
      "duplicate message discriminator 'A'",
      "duplicate case-sensitive message discriminator");

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields.push_back(
            source.messages[0].fields.back());
      },
      "FFSCHEMA010", "schema.messages.event.fields.payload.name",
      "duplicate field name 'payload'", "duplicate field name");

  compiler::schema_source case_sensitive = base;
  compiler::message_source lowercase = case_sensitive.messages.front();
  lowercase.name = "lowercase_event";
  lowercase.type = "a";
  lowercase.fields[0].value = "a";
  case_sensitive.messages.push_back(std::move(lowercase));
  tests.check(compiler::validate_schema(case_sensitive).has_value(),
              "uppercase and lowercase discriminators remain distinct");
}

void test_schema_and_field_layout(support::suite& tests) {
  const compiler::schema_source base = support::minimal_schema();

  support::expect_schema_error(
      tests, base, [](auto& source) { source.wire_endian = "little"; },
      "FFSCHEMA011", "schema.wire_endian",
      "wire_endian must be \"big\"", "unsupported wire endian");
  support::expect_schema_error(
      tests, base,
      [](auto& source) { source.discriminator_offset = 1; },
      "FFSCHEMA027", "schema.discriminator",
      "offset 0 and width 1", "schema discriminator offset mismatch");
  support::expect_schema_error(
      tests, base,
      [](auto& source) { source.discriminator_width = 2; },
      "FFSCHEMA027", "schema.discriminator",
      "offset 0 and width 1", "schema discriminator width mismatch");

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[1].offset = -1;
      },
      "FFSCHEMA021", "fields.payload.offset",
      "field offset must be non-negative",
      "negative field offset");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[1].offset =
            static_cast<std::int64_t>(
                std::numeric_limits<std::uint32_t>::max()) +
            1;
      },
      "FFSCHEMA021", "fields.payload.offset",
      "representable in 32 bits", "field offset above representation bound");
  support::expect_schema_error(
      tests, base,
      [](auto& source) { source.messages[0].fields[1].width = 2; },
      "FFSCHEMA022", "fields.payload",
      "field ends at byte 3, beyond declared size 2",
      "field extends beyond message boundary");
  support::expect_schema_error(
      tests, base,
      [](auto& source) { source.messages[0].fields[1].offset = 0; },
      "FFSCHEMA023", "fields.payload",
      "field overlaps a preceding field", "overlapping fields");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[1].offset = 2;
        source.messages[0].size = 3;
      },
      "FFSCHEMA024", "fields.payload",
      "undeclared byte gap begins at offset 1",
      "undeclared byte gap");
  support::expect_schema_error(
      tests, base,
      [](auto& source) { source.messages[0].size = 3; },
      "FFSCHEMA025", "schema.messages.event.fields",
      "declared fields end at byte 2, before declared size 3",
      "final coverage does not reach message size");

  compiler::schema_source reserved_coverage = base;
  reserved_coverage.messages[0].size = 4;
  reserved_coverage.messages[0].fields.push_back(compiler::field_source{
      .mark = support::test_mark(),
      .name = "explicit_padding",
      .type = "reserved",
      .offset = 2,
      .width = 2,
      .role = std::nullopt,
      .value = std::nullopt,
      .allowed = {},
  });
  tests.check(compiler::validate_schema(reserved_coverage).has_value(),
              "explicit reserved range closes byte coverage");

  compiler::schema_source declaration_order = reserved_coverage;
  std::ranges::reverse(declaration_order.messages[0].fields);
  tests.check(compiler::validate_schema(declaration_order).has_value(),
              "field declaration order does not alter complete coverage");

  support::expect_schema_error(
      tests, base, [](auto& source) { source.messages.clear(); },
      "FFSCHEMA030", "schema.messages",
      "must define at least one message", "schema without messages");
  support::expect_schema_error(
      tests, base,
      [](auto& source) { source.messages[0].fields.clear(); },
      "FFSCHEMA031", "schema.messages.event.fields",
      "must define at least one field", "message without fields");
}

void test_discriminator_role_value_rules(support::suite& tests) {
  const compiler::schema_source base = support::minimal_schema();

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[0].role.reset();
        source.messages[0].fields[0].value.reset();
      },
      "FFSCHEMA026", "schema.messages.event.fields",
      "exactly one discriminator field",
      "message missing discriminator role and value");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[0].value.reset();
      },
      "FFSCHEMA035", "fields.message_type.value",
      "discriminator field requires value",
      "discriminator role missing required value");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[0].role.reset();
      },
      "FFSCHEMA035", "fields.message_type.value",
      "value is permitted only on the discriminator field",
      "discriminator value forbidden without role");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[1].role = "payload";
      },
      "FFSCHEMA035", "fields.payload.role",
      "field role must be \"discriminator\"",
      "field role other than discriminator");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[1].value = "X";
      },
      "FFSCHEMA035", "fields.payload.value",
      "value is permitted only on the discriminator field",
      "value forbidden on ordinary field");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[0].offset = 1;
      },
      "FFSCHEMA027", "fields.message_type",
      "must have offset 0 and width 1",
      "discriminator field at wrong offset");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[0].width = 2;
      },
      "FFSCHEMA027", "fields.message_type",
      "must have offset 0 and width 1",
      "discriminator field with wrong width");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[0].type = "u8";
      },
      "FFSCHEMA033", "fields.message_type.type",
      "must use an ASCII physical type",
      "discriminator field with non-ASCII physical type");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[0].value = "B";
      },
      "FFSCHEMA028", "fields.message_type.value",
      "disagrees with message type 'A'",
      "discriminator value and message type mismatch");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        auto& second = source.messages[0].fields[1];
        second.name = "second_discriminator";
        second.offset = 0;
        second.role = "discriminator";
        second.value = "A";
      },
      "FFSCHEMA026", "schema.messages.event.fields",
      "exactly one discriminator field",
      "message with multiple discriminator fields");

  tests.check(compiler::validate_schema(base).has_value(),
              "exactly one role/value discriminator combination validates");
}

}  // namespace

int main() {
  support::suite tests;
  test_message_type_and_size_bounds(tests);
  test_duplicate_names_and_discriminators(tests);
  test_schema_and_field_layout(tests);
  test_discriminator_role_value_rules(tests);
  return tests.finish("schema layout");
}
