#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace feedforge::compiler {

struct source_position {
  std::size_t line{};
  std::size_t column{};

  friend bool operator==(const source_position&, const source_position&) =
      default;
};

struct source_mark {
  std::optional<source_position> position;
};

struct diagnostic {
  std::string code;
  std::string path;
  std::optional<source_position> position;
  std::string object_path;
  std::string message;
  std::optional<std::string> hint;
};

template <class T>
using result = std::expected<T, diagnostic>;

[[nodiscard]] std::string normalise_source_path(std::string_view path);
[[nodiscard]] std::string format_diagnostic(const diagnostic& problem);

[[nodiscard]] diagnostic make_diagnostic(
    std::string code, std::string path, const source_mark& mark,
    std::string object_path, std::string message,
    std::optional<std::string> hint = std::nullopt);

}  // namespace feedforge::compiler
