#include "compiler_fuzz_support.hpp"

#include "emit_cpp.hpp"
#include "ir.hpp"
#include "lower.hpp"
#include "model.hpp"
#include "parse_toml.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] bool exercise_compile(const std::string_view input) {
  const std::size_t separator = input.find(feedforge::fuzz::compiler_case_separator);
  if (separator == std::string_view::npos) {
    return false;
  }

  const std::string_view schema_text = input.substr(0U, separator);
  const std::string_view pipeline_text =
      input.substr(separator + feedforge::fuzz::compiler_case_separator.size());
  auto schema_source =
      feedforge::compiler::parse_schema_toml(schema_text, "fuzz/compile-schema.toml");
  if (!schema_source) {
    return false;
  }
  auto schema = feedforge::compiler::validate_schema(*schema_source);
  if (!schema) {
    return false;
  }
  auto pipeline_source =
      feedforge::compiler::parse_pipeline_toml(pipeline_text, "fuzz/compile-pipeline.toml");
  if (!pipeline_source) {
    return false;
  }
  auto pipeline = feedforge::compiler::validate_pipeline(*pipeline_source, *schema);
  if (!pipeline) {
    return false;
  }

  const feedforge::compiler::ffir_v1 first_ir =
      feedforge::compiler::lower_to_ffir(*schema, *pipeline, "fuzz");
  const feedforge::compiler::ffir_v1 second_ir =
      feedforge::compiler::lower_to_ffir(*schema, *pipeline, "fuzz");
  const std::string first_json = feedforge::compiler::canonical_json(first_ir);
  const std::string second_json = feedforge::compiler::canonical_json(second_ir);
  feedforge::fuzz::require(first_json == second_json);

  auto first_cpp = feedforge::compiler::emit_cpp(first_ir);
  auto second_cpp = feedforge::compiler::emit_cpp(second_ir);
  feedforge::fuzz::require(first_cpp.has_value() == second_cpp.has_value());
  if (first_cpp) {
    feedforge::fuzz::require(*first_cpp == *second_cpp);
    return true;
  } else {
    feedforge::fuzz::require_same_diagnostic(first_cpp.error(), second_cpp.error());
    return false;
  }
}

} // namespace

int feedforge_fuzz_compiler_compile_input(const std::uint8_t* const data, const std::size_t size) {
  static_cast<void>(exercise_compile(feedforge::fuzz::as_text(data, size)));
  return 0;
}

#if defined(FEEDFORGE_FUZZ_STANDALONE)
int exercise_standalone_compile_input(const std::uint8_t* const data, const std::size_t size) {
  const std::string_view arbitrary = feedforge::fuzz::as_text(data, size);

  std::string schema_case;
  schema_case.reserve(arbitrary.size() + feedforge::fuzz::compiler_case_separator.size() +
                      feedforge::fuzz::valid_pipeline_toml.size());
  schema_case.append(arbitrary);
  schema_case.append(feedforge::fuzz::compiler_case_separator);
  schema_case.append(feedforge::fuzz::valid_pipeline_toml);
  static_cast<void>(exercise_compile(schema_case));

  std::string pipeline_case;
  pipeline_case.reserve(feedforge::fuzz::valid_schema_toml.size() +
                        feedforge::fuzz::compiler_case_separator.size() + arbitrary.size());
  pipeline_case.append(feedforge::fuzz::valid_schema_toml);
  pipeline_case.append(feedforge::fuzz::compiler_case_separator);
  pipeline_case.append(arbitrary);
  static_cast<void>(exercise_compile(pipeline_case));
  return 0;
}

int main() {
  const std::string& valid = feedforge::fuzz::valid_compiler_case();
  feedforge::fuzz::require(exercise_compile(valid));
  return feedforge::fuzz::run_standalone_smoke(exercise_standalone_compile_input);
}
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
  return feedforge_fuzz_compiler_compile_input(data, size);
}
#endif
