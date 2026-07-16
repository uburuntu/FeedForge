#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "diagnostics.hpp"
#include "model.hpp"
#include "parse_toml.hpp"

namespace feedforge::compiler::test {

namespace fs = std::filesystem;

inline constexpr std::string_view valid_schema_toml = R"(format_version = 1
name = "test_feed"
protocol_version = "1.0"
document_revision = "2026-07-14"
wire_endian = "big"
discriminator_offset = 0
discriminator_width = 1
description = "Focused validation fixture"

[[types]]
name = "payload_type"
kind = "ascii"
width = 1
logical = "ascii"
description = "Payload alias"

[[messages]]
name = "event"
type = "A"
size = 2
description = "Event"
spec_section = "fixture"
spec_page = 1

[[messages.fields]]
name = "message_type"
type = "alpha"
offset = 0
width = 1
role = "discriminator"
value = "A"
description = "Discriminator"
spec_section = "fixture"
spec_page = 1

[[messages.fields]]
name = "payload"
type = "payload_type"
offset = 1
width = 1
allowed = ["X"]
description = "Payload"
spec_section = "fixture"
spec_page = 1
)";

inline constexpr std::string_view valid_pipeline_toml = R"(format_version = 1
name = "test_pipeline"
namespace = "FeedForge::generated::test_pipeline"
schema = "test_feed"
profile = "portable_checked"
unknown_messages = "error"
unselected_messages = "skip"

[[emit]]
source = "A"
event = "event"
fields = ["payload"]
)";

class suite {
 public:
  void check(const bool condition, const std::string_view description) {
    if (!condition) {
      std::cerr << "FAIL: " << description << '\n';
      ++failures_;
    }
  }

  template <class T>
  void expect_error(const result<T>& actual, const std::string_view code,
                    const std::string_view object_fragment,
                    const std::string_view message_fragment,
                    const std::string_view description,
                    const std::optional<std::string_view> path =
                        std::nullopt) {
    check(!actual, std::string(description) + " is rejected");
    if (actual) {
      return;
    }
    const diagnostic& problem = actual.error();
    check(problem.code == code,
          std::string(description) + " has diagnostic code " +
              std::string(code));
    check(problem.object_path.find(object_fragment) != std::string::npos,
          std::string(description) + " identifies object " +
              std::string(object_fragment));
    check(problem.message.find(message_fragment) != std::string::npos,
          std::string(description) + " explains " +
              std::string(message_fragment));
    check(!problem.message.empty(),
          std::string(description) + " has useful diagnostic text");
    if (path) {
      check(problem.path == *path,
            std::string(description) + " preserves normalized source path");
    }
  }

  [[nodiscard]] int finish(const std::string_view name) const {
    if (failures_ != 0) {
      std::cerr << failures_ << ' ' << name << " test(s) failed\n";
      return 1;
    }
    return 0;
  }

 private:
  int failures_{};
};

[[nodiscard]] inline std::string read_text(const fs::path& path) {
  std::ifstream input{path, std::ios::binary};
  std::ostringstream contents;
  contents << input.rdbuf();
  return std::move(contents).str();
}

[[nodiscard]] inline std::string replace_once(
    suite& tests, std::string input, const std::string_view from,
    const std::string_view to, const std::string_view description) {
  const std::size_t position = input.find(from);
  tests.check(position != std::string::npos,
              std::string(description) + " mutation source exists");
  if (position != std::string::npos) {
    input.replace(position, from.size(), to);
  }
  return input;
}

[[nodiscard]] inline std::string erase_once(
    suite& tests, std::string input, const std::string_view text,
    const std::string_view description) {
  return replace_once(tests, std::move(input), text, "", description);
}

[[nodiscard]] inline source_mark test_mark(const std::size_t line = 1U,
                                           const std::size_t column = 1U) {
  return source_mark{source_position{line, column}};
}

[[nodiscard]] inline schema_source minimal_schema() {
  const source_mark mark = test_mark(4U, 3U);
  schema_source source{
      .mark = mark,
      .source_path = "fixtures/schema.toml",
      .format_version = 1,
      .name = "test_feed",
      .protocol_version = "1.0",
      .document_revision = "2026-07-14",
      .wire_endian = "big",
      .discriminator_offset = 0,
      .discriminator_width = 1,
      .types = {},
      .messages = {},
  };
  source.messages.push_back(message_source{
      .mark = mark,
      .name = "event",
      .type = "A",
      .size = 2,
      .fields =
          {
              field_source{
                  .mark = mark,
                  .name = "message_type",
                  .type = "alpha",
                  .offset = 0,
                  .width = 1,
                  .role = "discriminator",
                  .value = "A",
                  .allowed = {},
              },
              field_source{
                  .mark = mark,
                  .name = "payload",
                  .type = "alpha",
                  .offset = 1,
                  .width = 1,
                  .role = std::nullopt,
                  .value = std::nullopt,
                  .allowed = {},
              },
          },
  });
  return source;
}

[[nodiscard]] inline pipeline_source minimal_pipeline() {
  const source_mark mark = test_mark(5U, 7U);
  pipeline_source source{
      .mark = mark,
      .source_path = "fixtures/pipeline.toml",
      .format_version = 1,
      .name = "test_pipeline",
      .cpp_namespace = "FeedForge::generated::test_pipeline",
      .schema = "test_feed",
      .profile = "portable_checked",
      .unknown_messages = "error",
      .unselected_messages = "skip",
      .projections = {},
  };
  source.projections.push_back(projection_source{
      .mark = mark,
      .source = "A",
      .event = "event",
      .fields = {"payload"},
      .field_marks = {mark},
  });
  return source;
}

template <class Mutation>
void expect_schema_error(suite& tests, const schema_source& base,
                         Mutation mutation, const std::string_view code,
                         const std::string_view object_fragment,
                         const std::string_view message_fragment,
                         const std::string_view description) {
  schema_source candidate = base;
  mutation(candidate);
  tests.expect_error(validate_schema(candidate), code, object_fragment,
                     message_fragment, description,
                     std::string_view{"fixtures/schema.toml"});
}

template <class Mutation>
void expect_pipeline_error(suite& tests, const pipeline_source& base,
                           const resolved_schema& schema, Mutation mutation,
                           const std::string_view code,
                           const std::string_view object_fragment,
                           const std::string_view message_fragment,
                           const std::string_view description) {
  pipeline_source candidate = base;
  mutation(candidate);
  tests.expect_error(validate_pipeline(candidate, schema), code,
                     object_fragment, message_fragment, description,
                     std::string_view{"fixtures/pipeline.toml"});
}

}  // namespace feedforge::compiler::test
