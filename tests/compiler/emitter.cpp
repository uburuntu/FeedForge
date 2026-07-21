#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "emit_cpp.hpp"
#include "feedforge/version.hpp"
#include "lower.hpp"
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

void check_contains(const std::string_view output,
                    const std::string_view expected,
                    const std::string_view message) {
  check(output.find(expected) != std::string_view::npos, message);
}

[[nodiscard]] std::string read_text(const fs::path& path) {
  std::ifstream input{path, std::ios::binary};
  std::ostringstream contents;
  contents << input.rdbuf();
  return std::move(contents).str();
}

[[nodiscard]] compiler::ffir_field_v1 make_field(
    std::string name, const compiler::physical_kind physical,
    const compiler::logical_kind logical, const std::uint32_t offset,
    const std::uint32_t width,
    const std::optional<std::uint32_t> scale = std::nullopt) {
  return compiler::ffir_field_v1{
      .name = std::move(name),
      .type_name = "resolved_type",
      .physical = physical,
      .logical = logical,
      .offset = offset,
      .width = width,
      .scale = scale,
      .discriminator = false,
      .projectable = true,
  };
}

[[nodiscard]] compiler::ffir_v1 mapping_ir() {
  std::vector<compiler::ffir_field_v1> fields;
  fields.push_back(make_field("raw_1", compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::raw_unsigned, 1U, 1U));
  fields.push_back(make_field("raw_2", compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::raw_unsigned, 2U, 2U));
  fields.push_back(make_field("raw_4", compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::raw_unsigned, 4U, 4U));
  fields.push_back(make_field("raw_6", compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::raw_unsigned, 8U, 6U));
  fields.push_back(make_field("raw_8", compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::raw_unsigned, 14U, 8U));
  fields.push_back(make_field("text", compiler::physical_kind::ascii,
                              compiler::logical_kind::ascii, 22U, 3U));
  fields.push_back(make_field("timestamp",
                              compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::timestamp_ns, 25U, 6U));
  fields.push_back(make_field("price_4",
                              compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::decimal, 31U, 4U, 4U));
  fields.push_back(make_field("price_8",
                              compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::decimal, 35U, 8U, 8U));
  fields.push_back(make_field("locate",
                              compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::stock_locate, 43U, 2U));
  fields.push_back(make_field("tracking",
                              compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::tracking_number, 45U, 2U));
  fields.push_back(make_field(
      "order_reference", compiler::physical_kind::unsigned_integer,
      compiler::logical_kind::order_reference_number, 47U, 8U));
  fields.push_back(make_field("match",
                              compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::match_number, 55U, 8U));
  fields.push_back(make_field("shares",
                              compiler::physical_kind::unsigned_integer,
                              compiler::logical_kind::share_count, 63U, 4U));

  std::vector<compiler::ffir_field_v1> schema_fields;
  schema_fields.push_back(compiler::ffir_field_v1{
      .name = "message_type",
      .type_name = "alpha",
      .physical = compiler::physical_kind::ascii,
      .logical = compiler::logical_kind::ascii,
      .offset = 0U,
      .width = 1U,
      .scale = {},
      .discriminator = true,
      .projectable = false,
  });
  schema_fields.insert(schema_fields.end(), fields.begin(), fields.end());

  compiler::ffir_v1 ir{
      .format_version = compiler::ffir_format_version,
      .generator_version = "generator\"\\\nversion",
      .schema =
          {
              .name = "mapping_schema",
              .protocol_version = "1.0",
              .document_revision = "revision\nline",
              .fingerprint = std::string(64U, 'a'),
              .wire_endian = "big",
              .discriminator_offset = 0U,
              .discriminator_width = 1U,
              .types = {},
              .messages =
                  {
                      compiler::ffir_message_v1{
                          .name = "mapping_message",
                          .discriminator = 0x4dU,
                          .size = 67U,
                          .fields = std::move(schema_fields),
                      },
                  },
          },
      .pipeline =
          {
              .name = "mapping_pipeline",
              .cpp_namespace = "feedforge::generated::mapping_pipeline",
              .schema = "mapping_schema",
              .profile = "portable_checked",
              .variant_id = "portable_checked.v1",
              .unknown_messages = "error",
              .unselected_messages = "skip",
              .fingerprint = std::string(64U, 'b'),
              .events =
                  {
                      compiler::ffir_event_v1{
                          .event = "mapping_event",
                          .source_discriminator = 0x4dU,
                          .source_message = "mapping_message",
                          .fields = std::move(fields),
                      },
                  },
          },
  };
  ir.schema.fingerprint =
      compiler::sha256_hex(compiler::canonical_schema_semantics_json(ir));
  ir.pipeline.fingerprint =
      compiler::sha256_hex(compiler::canonical_pipeline_semantics_json(ir));
  return ir;
}

void test_golden_and_determinism() {
  const fs::path fixtures{FEEDFORGE_COMPILER_FIXTURE_DIR};
  const auto schema_source = compiler::parse_schema_file(
      (fixtures / "valid_schema.toml").generic_string());
  const auto pipeline_source = compiler::parse_pipeline_file(
      (fixtures / "valid_pipeline.toml").generic_string());
  const auto reordered_schema_source = compiler::parse_schema_file(
      (fixtures / "valid_schema_reordered.toml").generic_string());
  const auto reordered_pipeline_source = compiler::parse_pipeline_file(
      (fixtures / "valid_pipeline_reordered.toml").generic_string());
  check(schema_source.has_value() && pipeline_source.has_value() &&
            reordered_schema_source.has_value() &&
            reordered_pipeline_source.has_value(),
        "emitter fixtures parse");
  if (!schema_source || !pipeline_source || !reordered_schema_source ||
      !reordered_pipeline_source) {
    return;
  }

  const auto schema = compiler::validate_schema(*schema_source);
  const auto reordered_schema =
      compiler::validate_schema(*reordered_schema_source);
  const auto pipeline =
      schema ? compiler::validate_pipeline(*pipeline_source, *schema)
             : compiler::result<compiler::resolved_pipeline>{
                   std::unexpected(compiler::diagnostic{})};
  const auto reordered_pipeline =
      reordered_schema
          ? compiler::validate_pipeline(*reordered_pipeline_source,
                                        *reordered_schema)
          : compiler::result<compiler::resolved_pipeline>{
                std::unexpected(compiler::diagnostic{})};
  check(schema.has_value() && pipeline.has_value() &&
            reordered_schema.has_value() && reordered_pipeline.has_value(),
        "emitter fixtures validate");
  if (!schema || !pipeline || !reordered_schema || !reordered_pipeline) {
    return;
  }

  const compiler::ffir_v1 ir = compiler::lower_to_ffir(
      *schema, *pipeline, feedforge::version_string);
  const compiler::ffir_v1 reordered = compiler::lower_to_ffir(
      *reordered_schema, *reordered_pipeline, feedforge::version_string);
  const auto first = compiler::emit_cpp(ir);
  const auto second = compiler::emit_cpp(ir);
  const auto reordered_output = compiler::emit_cpp(reordered);
  check(first.has_value() && second.has_value() &&
            reordered_output.has_value(),
        "valid resolved FFIR emits C++");
  if (!first || !second || !reordered_output) {
    return;
  }

  check(*first == *second, "repeat emission is byte-identical");
  check(*first == *reordered_output,
        "semantic TOML reordering is byte-identical");
  check(*first ==
            read_text(fs::path{FEEDFORGE_COMPILER_GOLDEN_DIR} /
                      "synthetic_pipeline.hpp"),
        "emitter output matches committed golden bytes");
  check(first->find(FEEDFORGE_COMPILER_FIXTURE_DIR) == std::string::npos,
        "generated source omits absolute source paths");
  check(first->find("order_id") == std::string::npos,
        "unprojected field emits neither member nor load");
  check(first->find("padding") == std::string::npos,
        "reserved field emits neither member nor load");
  check_contains(*first, "load_unsigned<4U>(payload.data() + 3U)",
                 "projected decimal load is emitted");
  check_contains(*first,
                 "std::memcpy(event.side.raw.data(), payload.data() + 7U, 1U)",
                 "projected ASCII copy is emitted");
  check_contains(*first, "class chunked_replayer", "caller-buffered chunked replay is emitted");
  check_contains(*first, "cursor_.next(chunk.subspan(position))",
                 "chunked replay consumes arbitrary chunk boundaries");
  check_contains(*first, "static_assert(feedforge::runtime_api_epoch ==",
                 "generated header requires an exact runtime API epoch");
  check_contains(*first, "static_assert(feedforge::runtime_api_revision >=",
                 "generated header accepts a compatible newer runtime revision");
  check(first->find("runtime_api_version") == std::string::npos,
        "generated header does not retain the superseded exact API version");

  compiler::ffir_v1 skip_ir = ir;
  skip_ir.pipeline.unknown_messages = "skip";
  skip_ir.pipeline.fingerprint = compiler::sha256_hex(
      compiler::canonical_pipeline_semantics_json(skip_ir));
  const auto skip_output = compiler::emit_cpp(skip_ir);
  check(skip_output.has_value(), "unknown-skip FFIR emits");
  if (skip_output) {
    check_contains(*skip_output, "feedforge::decode_status::unknown_skipped",
                   "unknown skip policy is emitted directly");
    check(skip_output->find("decode_status::unknown_message_type") ==
              std::string::npos,
          "unknown error branch is not emitted for skip policy");
  }
}

void test_exact_value_mappings_and_escaping() {
  const auto emitted = compiler::emit_cpp(mapping_ir());
  check(emitted.has_value(), "all exact value mappings emit");
  if (!emitted) {
    return;
  }

  for (const std::string_view expected :
       {"std::uint8_t raw_1{};", "std::uint16_t raw_2{};",
        "std::uint32_t raw_4{};", "std::uint64_t raw_6{};",
        "std::uint64_t raw_8{};", "feedforge::ascii<3U> text{};",
        "feedforge::timestamp_ns timestamp{};",
        "feedforge::decimal<std::uint32_t, 4> price_4{};",
        "feedforge::decimal<std::uint64_t, 8> price_8{};",
        "feedforge::stock_locate locate{};",
        "feedforge::tracking_number tracking{};",
        "feedforge::order_reference_number order_reference{};",
        "feedforge::match_number match{};",
        "feedforge::share_count shares{};"}) {
    check_contains(*emitted, expected,
                   std::string("exact mapping emitted: ") +
                       std::string(expected));
  }
  for (const std::string_view width :
       {"<1U>", "<2U>", "<4U>", "<6U>", "<8U>"}) {
    check_contains(*emitted, "load_unsigned" + std::string(width),
                   std::string("unsigned load width emitted: ") +
                       std::string(width));
  }
  check_contains(*emitted,
                 "generator_version{\"generator\\\"\\\\\\nversion\"}",
                 "C++ string literals are escaped");
  check_contains(*emitted, "document revision revision\\x0Aline",
                 "generated comments cannot inject source lines");
}

void test_emission_invariants() {
  compiler::ffir_v1 invalid = mapping_ir();
  invalid.pipeline.events[0].event = "decoder";
  invalid.pipeline.fingerprint = compiler::sha256_hex(
      compiler::canonical_pipeline_semantics_json(invalid));
  const auto collision = compiler::emit_cpp(invalid);
  check(!collision && collision.error().code == "FFEMIT001",
        "generated declaration collision has stable emitter diagnostic");

  invalid = mapping_ir();
  invalid.pipeline.events[0].event = "chunked_replayer";
  invalid.pipeline.fingerprint =
      compiler::sha256_hex(compiler::canonical_pipeline_semantics_json(invalid));
  const auto chunked_collision = compiler::emit_cpp(invalid);
  check(!chunked_collision && chunked_collision.error().code == "FFEMIT001",
        "chunked replay declaration collision has stable diagnostic");

  invalid = mapping_ir();
  invalid.pipeline.events[0].fields[0].width = 3U;
  invalid.schema.messages[0].fields[1].width = 3U;
  invalid.schema.fingerprint =
      compiler::sha256_hex(compiler::canonical_schema_semantics_json(invalid));
  invalid.pipeline.fingerprint = compiler::sha256_hex(
      compiler::canonical_pipeline_semantics_json(invalid));
  const auto unsupported = compiler::emit_cpp(invalid);
  check(!unsupported && unsupported.error().code == "FFEMIT001",
        "unsupported projected mapping has stable emitter diagnostic");
}

}  // namespace

int main() {
  test_golden_and_determinism();
  test_exact_value_mappings_and_escaping();
  test_emission_invariants();

  if (failures != 0) {
    std::cerr << failures << " emitter test(s) failed\n";
    return 1;
  }
  return 0;
}
