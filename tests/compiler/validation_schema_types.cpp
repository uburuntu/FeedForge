#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "validation_test_support.hpp"

namespace {

namespace compiler = feedforge::compiler;
namespace support = feedforge::compiler::test;

[[nodiscard]] compiler::schema_source schema_with_type(
    const std::string_view kind, const std::int64_t width,
    const std::optional<std::string_view> logical,
    const std::optional<std::int64_t> scale = std::nullopt) {
  compiler::schema_source source = support::minimal_schema();
  source.types.push_back(compiler::type_source{
      .mark = support::test_mark(8U, 3U),
      .name = "semantic",
      .kind = std::string(kind),
      .width = width,
      .logical =
          logical ? std::optional<std::string>{std::string(*logical)}
                  : std::nullopt,
      .scale = scale,
  });
  if (width > 0 && width <= 65534) {
    source.messages[0].fields[1].type = "semantic";
    source.messages[0].fields[1].width = width;
    source.messages[0].size = width + 1;
  }
  return source;
}

void test_supported_semantic_types(support::suite& tests) {
  struct type_case {
    std::string_view name;
    std::string_view kind;
    std::int64_t width;
    std::optional<std::string_view> logical;
    std::optional<std::int64_t> scale;
    compiler::logical_kind expected;
  };
  constexpr std::array cases{
      type_case{"uint default", "uint", 1, std::nullopt, std::nullopt,
                compiler::logical_kind::raw_unsigned},
      type_case{"ascii default", "ascii", 3, std::nullopt, std::nullopt,
                compiler::logical_kind::ascii},
      type_case{"raw u8", "uint", 1, "raw_unsigned", std::nullopt,
                compiler::logical_kind::raw_unsigned},
      type_case{"raw u16", "uint", 2, "raw_unsigned", std::nullopt,
                compiler::logical_kind::raw_unsigned},
      type_case{"raw u32", "uint", 4, "raw_unsigned", std::nullopt,
                compiler::logical_kind::raw_unsigned},
      type_case{"raw u48", "uint", 6, "raw_unsigned", std::nullopt,
                compiler::logical_kind::raw_unsigned},
      type_case{"raw u64", "uint", 8, "raw_unsigned", std::nullopt,
                compiler::logical_kind::raw_unsigned},
      type_case{"timestamp", "uint", 6, "timestamp_ns", std::nullopt,
                compiler::logical_kind::timestamp_ns},
      type_case{"decimal u32 scale zero", "uint", 4, "decimal", 0,
                compiler::logical_kind::decimal},
      type_case{"decimal u64 scale eighteen", "uint", 8, "decimal", 18,
                compiler::logical_kind::decimal},
      type_case{"stock locate", "uint", 2, "stock_locate", std::nullopt,
                compiler::logical_kind::stock_locate},
      type_case{"tracking number", "uint", 2, "tracking_number", std::nullopt,
                compiler::logical_kind::tracking_number},
      type_case{"order reference", "uint", 8, "order_reference_number",
                std::nullopt, compiler::logical_kind::order_reference_number},
      type_case{"match number", "uint", 8, "match_number", std::nullopt,
                compiler::logical_kind::match_number},
      type_case{"share count", "uint", 4, "share_count", std::nullopt,
                compiler::logical_kind::share_count},
      type_case{"ascii", "ascii", 5, "ascii", std::nullopt,
                compiler::logical_kind::ascii},
  };

  for (const type_case& item : cases) {
    const auto resolved = compiler::validate_schema(schema_with_type(
        item.kind, item.width, item.logical, item.scale));
    tests.check(resolved.has_value(),
                std::string(item.name) + " semantic type validates");
    if (resolved) {
      tests.check(resolved->types.size() == 1U &&
                      resolved->types.front().logical == item.expected,
                  std::string(item.name) +
                      " resolves to the required logical kind");
      tests.check(resolved->messages.front().fields[1].width ==
                      static_cast<std::uint32_t>(item.width),
                  std::string(item.name) +
                      " preserves the compatible physical width");
    }
  }

  compiler::schema_source maximum_ascii = support::minimal_schema();
  maximum_ascii.types.push_back(compiler::type_source{
      .mark = support::test_mark(),
      .name = "maximum_ascii",
      .kind = "ascii",
      .width = 65535,
      .logical = "ascii",
      .scale = std::nullopt,
  });
  tests.check(compiler::validate_schema(maximum_ascii).has_value(),
              "maximum representable positive ascii type width validates");
}

void test_builtin_physical_types(support::suite& tests) {
  struct builtin_case {
    std::string_view name;
    std::int64_t width;
    bool projectable;
  };
  constexpr std::array cases{
      builtin_case{"u8", 1, true},       builtin_case{"u16", 2, true},
      builtin_case{"u32", 4, true},      builtin_case{"u48", 6, true},
      builtin_case{"u64", 8, true},      builtin_case{"alpha", 7, true},
      builtin_case{"reserved", 7, false},
  };
  for (const builtin_case& item : cases) {
    compiler::schema_source source = support::minimal_schema();
    source.messages[0].fields[1].type = item.name;
    source.messages[0].fields[1].width = item.width;
    source.messages[0].size = item.width + 1;
    const auto resolved = compiler::validate_schema(source);
    tests.check(resolved.has_value(),
                std::string("built-in ") + std::string(item.name) +
                    " accepts its required width form");
    if (resolved) {
      tests.check(resolved->messages.front().fields[1].projectable ==
                      item.projectable,
                  std::string("built-in ") + std::string(item.name) +
                      " has required projectability");
    }
  }
}

void test_kind_width_and_scale_rejections(support::suite& tests) {
  const compiler::schema_source base = support::minimal_schema();

  for (const std::string_view kind : {"float", "signed", "other_type"}) {
    support::expect_schema_error(
        tests, base,
        [kind](auto& source) {
          source.types.push_back(compiler::type_source{
              .mark = support::test_mark(),
              .name = "semantic",
              .kind = std::string(kind),
              .width = 4,
              .logical = std::nullopt,
              .scale = std::nullopt,
          });
        },
        "FFSCHEMA012", "schema.types.semantic.kind",
        "unsupported physical kind",
        std::string("unsupported physical kind ") + std::string(kind));
  }

  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.types.push_back(compiler::type_source{
            .mark = support::test_mark(),
            .name = "semantic",
            .kind = "uint",
            .width = 4,
            .logical = "currency",
            .scale = std::nullopt,
        });
      },
      "FFSCHEMA013", "schema.types.semantic.logical",
      "unsupported logical type", "unsupported logical type");

  struct width_case {
    std::int64_t width;
    std::string_view message;
    std::string_view description;
  };
  constexpr std::array widths{
      width_case{0, "type width must be positive", "zero type width"},
      width_case{-1, "type width must be positive", "negative type width"},
      width_case{3, "unsigned integer width must be",
                 "unsupported uint width"},
      width_case{65536, "no greater than 65535",
                 "type width above representation bound"},
  };
  for (const width_case& item : widths) {
    const compiler::schema_source candidate =
        schema_with_type("uint", item.width, "raw_unsigned");
    tests.expect_error(compiler::validate_schema(candidate), "FFSCHEMA014",
                       "schema.types.semantic.width", item.message,
                       item.description,
                       std::string_view{"fixtures/schema.toml"});
  }

  struct scale_case {
    std::optional<std::int64_t> scale;
    std::string_view description;
  };
  constexpr std::array decimal_scales{
      scale_case{std::nullopt, "decimal type missing required scale"},
      scale_case{-1, "decimal scale below zero"},
      scale_case{19, "decimal scale above eighteen"},
  };
  for (const scale_case& item : decimal_scales) {
    const compiler::schema_source candidate =
        schema_with_type("uint", 4, "decimal", item.scale);
    tests.expect_error(compiler::validate_schema(candidate), "FFSCHEMA015",
                       "schema.types.semantic.scale",
                       "require an integer scale in [0, 18]",
                       item.description,
                       std::string_view{"fixtures/schema.toml"});
  }

  for (const auto& [kind, logical] :
       {std::pair{std::string_view{"uint"}, std::string_view{"raw_unsigned"}},
        std::pair{std::string_view{"ascii"}, std::string_view{"ascii"}}}) {
    const compiler::schema_source candidate =
        schema_with_type(kind, 1, logical, 4);
    tests.expect_error(
        compiler::validate_schema(candidate), "FFSCHEMA015",
        "schema.types.semantic.scale",
        "scale is permitted only for the decimal logical type",
        std::string("forbidden scale on ") + std::string(logical),
        std::string_view{"fixtures/schema.toml"});
  }
}

void test_physical_logical_incompatibilities(support::suite& tests) {
  struct incompatible_case {
    std::string_view name;
    std::string_view kind;
    std::int64_t width;
    std::string_view logical;
    std::optional<std::int64_t> scale;
  };
  constexpr std::array cases{
      incompatible_case{"raw unsigned on ascii", "ascii", 1, "raw_unsigned",
                        std::nullopt},
      incompatible_case{"ascii on uint", "uint", 1, "ascii", std::nullopt},
      incompatible_case{"timestamp wrong width", "uint", 4, "timestamp_ns",
                        std::nullopt},
      incompatible_case{"timestamp wrong physical", "ascii", 6,
                        "timestamp_ns", std::nullopt},
      incompatible_case{"decimal wrong width", "uint", 2, "decimal", 4},
      incompatible_case{"decimal wrong physical", "ascii", 4, "decimal", 4},
      incompatible_case{"stock locate wrong width", "uint", 4,
                        "stock_locate", std::nullopt},
      incompatible_case{"stock locate wrong physical", "ascii", 2,
                        "stock_locate", std::nullopt},
      incompatible_case{"tracking number wrong width", "uint", 4,
                        "tracking_number", std::nullopt},
      incompatible_case{"tracking number wrong physical", "ascii", 2,
                        "tracking_number", std::nullopt},
      incompatible_case{"order reference wrong width", "uint", 4,
                        "order_reference_number", std::nullopt},
      incompatible_case{"order reference wrong physical", "ascii", 8,
                        "order_reference_number", std::nullopt},
      incompatible_case{"match number wrong width", "uint", 4, "match_number",
                        std::nullopt},
      incompatible_case{"match number wrong physical", "ascii", 8,
                        "match_number", std::nullopt},
      incompatible_case{"share count wrong width", "uint", 2, "share_count",
                        std::nullopt},
      incompatible_case{"share count wrong physical", "ascii", 4,
                        "share_count", std::nullopt},
  };
  for (const incompatible_case& item : cases) {
    const compiler::schema_source candidate = schema_with_type(
        item.kind, item.width, item.logical, item.scale);
    tests.expect_error(
        compiler::validate_schema(candidate), "FFSCHEMA016",
        "schema.types.semantic", "incompatible with", item.name,
        std::string_view{"fixtures/schema.toml"});
  }
}

void test_field_type_rules(support::suite& tests) {
  const compiler::schema_source base = support::minimal_schema();

  support::expect_schema_error(
      tests, base,
      [](auto& source) { source.messages[0].fields[1].type = "missing"; },
      "FFSCHEMA019", "fields.payload.type", "undeclared type 'missing'",
      "field references undeclared type");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[1].type = "u16";
        source.messages[0].fields[1].width = 1;
      },
      "FFSCHEMA020", "fields.payload.width",
      "field width 1 disagrees with type width 2",
      "fixed-width field disagrees with built-in type");

  for (const std::int64_t width : {std::int64_t{0}, std::int64_t{-1},
                                   std::int64_t{65536}}) {
    support::expect_schema_error(
        tests, base,
        [width](auto& source) {
          source.messages[0].fields[1].width = width;
        },
        "FFSCHEMA014", "fields.payload.width",
        "field width must be in [1, 65535]",
        std::string("field width rejected: ") + std::to_string(width));
  }

  compiler::schema_source allowed = base;
  allowed.messages[0].fields[1].allowed = {"X", "?"};
  tests.check(compiler::validate_schema(allowed).has_value(),
              "same-width allowed metadata accepts unlisted alpha values");
  support::expect_schema_error(
      tests, base,
      [](auto& source) {
        source.messages[0].fields[1].allowed = {"XX"};
      },
      "FFSCHEMA034", "fields.payload.allowed",
      "exactly 1 bytes", "allowed metadata width mismatch");
}

}  // namespace

int main() {
  support::suite tests;
  test_supported_semantic_types(tests);
  test_builtin_physical_types(tests);
  test_kind_width_and_scale_rejections(tests);
  test_physical_logical_incompatibilities(tests);
  test_field_type_rules(tests);
  return tests.finish("schema type");
}
