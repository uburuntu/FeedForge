#include "compiler_fuzz_support.hpp"

#include "model.hpp"
#include "parse_toml.hpp"

#include <cstddef>
#include <cstdint>

namespace {

[[nodiscard]] bool exercise_schema(const std::string_view text) {
  auto first = feedforge::compiler::parse_schema_toml(text, "fuzz/schema.toml");
  auto second = feedforge::compiler::parse_schema_toml(text, "fuzz/schema.toml");
  feedforge::fuzz::require(first.has_value() == second.has_value());
  if (!first) {
    feedforge::fuzz::require_same_diagnostic(first.error(), second.error());
    return false;
  }

  auto first_validated = feedforge::compiler::validate_schema(*first);
  auto second_validated = feedforge::compiler::validate_schema(*second);
  feedforge::fuzz::require(first_validated.has_value() == second_validated.has_value());
  if (!first_validated) {
    feedforge::fuzz::require_same_diagnostic(first_validated.error(), second_validated.error());
    return false;
  }
  return true;
}

} // namespace

int feedforge_fuzz_compiler_schema_input(const std::uint8_t* const data, const std::size_t size) {
  static_cast<void>(exercise_schema(feedforge::fuzz::as_text(data, size)));
  return 0;
}

#if defined(FEEDFORGE_FUZZ_STANDALONE)
int main() {
  const auto valid = feedforge::fuzz::valid_schema_toml;
  feedforge::fuzz::require(exercise_schema(valid));
  return feedforge::fuzz::run_standalone_smoke(feedforge_fuzz_compiler_schema_input);
}
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
  return feedforge_fuzz_compiler_schema_input(data, size);
}
#endif
